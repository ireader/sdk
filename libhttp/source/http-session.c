#include "cstringext.h"
#include "http-server-internal.h"
#include "http-reason.h"
#include "http-parser.h"
#include "http-bundle.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#if defined(OS_WINDOWS)
#define iov_base buf
#define iov_len	 len
#endif

// create a new http session
static struct http_session_t* http_session_new()
{
	struct http_session_t* session;

	session = (struct http_session_t*)malloc(sizeof(session[0]));
	if(!session)
		return NULL;

	memset(session, 0, sizeof(session[0]));
	session->parser = http_parser_create(HTTP_PARSER_SERVER);
    locker_create(&session->locker);
    session->ref = 1;
	return session;
}

static void http_session_release(struct http_session_t *session)
{
    if(0 == atomic_decrement32(&session->ref))
    {
        if(session->parser)
            http_parser_destroy(session->parser);

        locker_destroy(&session->locker);

    #if defined(DEBUG) || defined(_DEBUG)
        memset(session, 0xCC, sizeof(*session));
    #endif
        free(session);
    }
}

// reuse/create a http session
static struct http_session_t* http_session_alloc()
{
	struct http_session_t* session;

	// TODO: reuse ? 
	session = http_session_new();
	if(!session)
		return NULL;

	return session;
}

static void http_session_handle(struct http_session_t *session)
{
	const char* uri = http_get_request_uri(session->parser);
	const char* method = http_get_request_method(session->parser);

	if(session->server->handle)
	{
		session->server->handle(session->server->param, session, method, uri);
	}
}

static void http_session_clear(struct http_session_t *session)
{
	int i;

	// release bundle data
	for(i = 2; i < session->vec_count; i++)
	{
		//http_bundle_free(session->vec[i].buf);
		http_bundle_free((struct http_bundle_t *)session->vec[i].iov_base - 1);
	}

	if(session->vec3 != session->vec)
		free(session->vec);
	session->vec = NULL;
	session->vec_count = 0;

	// session done
	http_session_release(session);
}

static int http_session_send(struct http_session_t *session, int idx)
{
	int r = -1;

    locker_lock(&session->locker);
    if(session->session)
    {
        r = aio_tcp_transport_sendv(session->session, session->vec + idx, session->vec_count-idx);
    }
    locker_unlock(&session->locker);
    return r;
}

int http_session_onrecv(void* param, const void* msg, size_t bytes)
{
	size_t remain;
	struct http_session_t *session;
	session = (struct http_session_t *)param;
	if(!session || 0 == bytes)
		return -1;

	assert(session && bytes > 0);
	remain = bytes;
	if(0 == http_parser_input(session->parser, msg, &remain))
	{
		session->data[0] = '\0'; // clear for save user-defined header

        atomic_increment32(&session->ref); // for http reply

		// call
		// user must reply(send/send_vec/send_file) in handle
		http_session_handle(session);

		// do restart in send done
		// http_session_onsend
		return 0;
	}
	else
	{
		return 1;
	}
}

int http_session_onsend(void* param, int code, size_t bytes)
{
	int i;
	char* ptr;
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	if(code < 0 || 0 == bytes)
	{
		http_session_clear(session);
        return 0; // send error, don't need recv
	}
	else
	{
		for(i = 0; i < session->vec_count && bytes > 0; i++)
		{
			if(bytes >= session->vec[i].iov_len)
			{
				bytes -= session->vec[i].iov_len;
				session->vec[i].iov_len = 0;
			}
			else
			{
				ptr = session->vec[i].iov_base;
				session->vec[i].iov_len -= bytes;
				memmove(ptr, ptr + bytes, session->vec[i].iov_len);
				bytes = 0;
				break;
			}
		}

		if(i < session->vec_count)
		{
			if(0 != http_session_send(session, i))
			{
				http_session_clear(session);
				// return -1;
			}
            return 0; // have more data to send, don't need recv
		}
		else
		{
			// restart
			// clear parser status
			http_parser_clear(session->parser);

			http_session_clear(session);

            return 1; // send done, continue recv data
		}
	}
}

void* http_session_onconnected(void* ptr, void* sid, const char* ip, int port)
{
	struct http_session_t *session;

	session = http_session_alloc();
	if(!session) return NULL;

	session->server = (struct http_server_t *)ptr;
	session->session = sid;
	memcpy(session->ip, ip, sizeof(session->ip)-1);
	session->ip[sizeof(session->ip)-1] = '\0';
	session->port = port;

	return session;
}

void http_session_ondisconnected(void* param)
{
	struct http_session_t *session;
	session = (	struct http_session_t *)param;
    locker_lock(&session->locker);
    session->session = NULL;
    locker_unlock(&session->locker);
	http_session_release(session);
}

// Request
int http_server_get_client(void* param, const char** ip, int *port)
{
	struct http_session_t *session;
	session = (struct http_session_t*)param;
	if(ip) *ip = session->ip;
	if(port) *port = session->port;
	return 0;
}

const char* http_server_get_header(void* param, const char *name)
{
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	return http_get_header_by_name(session->parser, name);
}

int http_server_get_content(void* param, void **content, size_t *length)
{
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	*content = (void*)http_get_content(session->parser);
	*length = http_get_content_length(session->parser);
	return 0;
}

int http_server_send(void* param, int code, void* bundle)
{
	return http_server_send_vec(param, code, &bundle, 1);
}

int http_server_send_vec(void* param, int code, void** bundles, int num)
{
	int i;
    size_t len;
	char msg[128];
	struct http_bundle_t *bundle;
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	assert(0 == session->vec_count);
	session->vec = (1 == num) ? session->vec3 : malloc(sizeof(socket_bufvec_t) * (num+2));
	if(!session->vec)
		return -1;

	session->vec_count = num + 2;

	// HTTP Response Data
	len = 0;
	for(i = 0; i < num; i++)
	{
		bundle = bundles[i];
		assert(bundle->len > 0);
		http_bundle_addref(bundle); // addref
		socket_setbufvec(session->vec, i+2, bundle->ptr, bundle->len);
		len += bundle->len;
	}

	// HTTP Response Header
	sprintf(msg, "Server: WebServer 0.2\r\n"
		"Connection: keep-alive\r\n"
		"Keep-Alive: timeout=5,max=100\r\n"
		"Content-Length: %u\r\n\r\n", (unsigned int)len);
	strcat(session->data, msg);
	sprintf(session->status_line, "HTTP/1.1 %d %s\r\n", code, http_reason_phrase(code));

	socket_setbufvec(session->vec, 0, session->status_line, strlen(session->status_line));
	socket_setbufvec(session->vec, 1, session->data, strlen(session->data));

	i = http_session_send(session, 0);
	if(0 != i)
		http_session_clear(session);
	return i;
}

int http_server_set_header(void* param, const char* name, const char* value)
{
	char msg[512];
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	snprintf(msg, sizeof(msg), "%s: %s\r\n", name, value);
	strcat(session->data, msg);
	return 0;
}

int http_server_set_header_int(void* param, const char* name, int value)
{
	char msg[512];
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	snprintf(msg, sizeof(msg), "%s: %d\r\n", name, value);
	strcat(session->data, msg);
	return 0;
}

int http_server_set_content_type(void* session, const char* value)
{
	//Content-Type: application/json
	//Content-Type: text/html; charset=utf-8
	return http_server_set_header(session, "Content-Type", value);
}
