#include "http-server-internal.h"
#include "aio-transport.h"
#include "http-reason.h"
#include "http-parser.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#if defined(OS_WINDOWS)
#define iov_base	buf
#define iov_len		len
#define strcasecmp	_stricmp
#endif

#define HTTP_RECV_BUFFER (2*1024)

static socket_bufvec_t* socket_bufvec_alloc(struct http_session_t *session, int count);
static int http_session_data(struct http_session_t *session, const struct http_vec_t* vec, int num, int reserved);

static const char* s_http_header_end = "\r\n";

static void http_session_ondestroy(void* param)
{
	struct http_session_t *session;
	session = (struct http_session_t *)param;
	session->transport = NULL;

	if (session->ws && session->ws->handler.ondestroy)
		session->ws->handler.ondestroy(session->ws->param);

	if (session->parser)
	{
		http_parser_destroy(session->parser);
		session->parser = NULL;
	}

	if (session->__vec)
	{
		free(session->__vec);
		session->__vec = NULL;
		session->vec_capacity = 0;
	}

	if (session->header != (char*)(session + 1))
	{
		assert(session->header_capacity > 2 * 1024);
		free(session->header);
		session->header = NULL;
		session->header_size = 0;
		session->header_capacity = 0;
	}

#if defined(DEBUG) || defined(_DEBUG)
	memset(session, 0xCC, sizeof(*session));
#endif
	free(session);
}

static void http_session_reset(struct http_session_t *session)
{
	session->header_size = 0;
	session->http_response_code_flag = 0;
	session->http_response_header_flag = 0;
	session->http_content_length_flag = 0;
	session->http_transfer_encoding_chunked_flag = 0;
}

static void http_session_onrecv(void* param, int code, size_t bytes)
{
	struct http_session_t *session;
	session = (struct http_session_t *)param;
	session->remain = 0 == code ? (int)bytes : 0;

	if (!session->ws)
	{
		if (0 == code)
		{
			code = http_parser_input(session->parser, session->data, &session->remain);
			if (0 == code)
			{
				// wait for next recv
				atomic_cas_ptr(&session->rlocker, session, NULL);

				// http pipeline remain data
				assert(bytes > session->remain);
				if (session->remain > 0 && bytes > session->remain)
					memmove(session->data, session->data + (bytes - session->remain), session->remain);

				// clear for save user-defined header
				http_session_reset(session);

				// call
				// user must reply(send/send_vec/send_file) in handle
				if (session->handler)
				{
					const char* uri = http_get_request_uri(session->parser);
					const char* method = http_get_request_method(session->parser);
					session->handler(session->param, session, method, uri);
				}
			}
			else if (code > 0)
			{
				// recv more data
				assert(0 == session->remain);
				code = aio_tcp_transport_recv(session->transport, session->data, HTTP_RECV_BUFFER);
			}
		}
	}
	else if(0 == code) // websocket
	{
		code = websocket_parser_input(&session->ws->parser, (uint8_t*)session->data, session->remain, session->ws->handler.ondata, session->ws->param);
		if (0 == code)
		{
			// recv more data
			code = aio_tcp_transport_recv(session->transport, session->data, HTTP_RECV_BUFFER);
		}
	}
	
	// error or peer closed
	if(0 != code || 0 == bytes)
	{
		code = aio_tcp_transport_destroy(session->transport);
	}	
}

static void http_session_onsend(void* param, int code, size_t bytes)
{
	int r = 0;
	struct http_session_t *session;
	struct http_websocket_t* websocket;
	session = (struct http_session_t*)param;
	session->vec_count = 0;
	session->vec = NULL;
	websocket = session->ws; // HACK: check before onsend callback
	if (session->onsend)
	{
		r = session->onsend(session->onsendparam, code, bytes);
		if (1 == r)
			return; // HACK: has more data to send(such as sendfile)
	}

	if (0 == r && 0 == code && NULL == websocket /* exclude websocket mode */ )
	{
		http_parser_clear(session->parser); // reset parser
		if (session->remain > 0)
			http_session_onrecv(session, 0, session->remain); // next round
		else if(atomic_cas_ptr(&session->rlocker, NULL, session))
			r = aio_tcp_transport_recv(session->transport, session->data, HTTP_RECV_BUFFER);
	}

	if (0 != code || 0 != r)
		code = aio_tcp_transport_destroy(session->transport);
}

int http_session_create(struct http_server_t *server, socket_t socket, const struct sockaddr* sa, socklen_t salen)
{
	struct http_session_t *session;
	struct aio_transport_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.ondestroy = http_session_ondestroy;
	handler.onrecv = http_session_onrecv;
	handler.onsend = http_session_onsend;

	session = (struct http_session_t *)malloc(sizeof(*session) + 2 * 1024 + HTTP_RECV_BUFFER);
	if (!session) return -1;

	memset(session, 0, sizeof(*session));
	session->header = (char*)(session + 1);
	session->header_capacity = 2 * 1024;
	session->data = session->header + session->header_capacity;
	session->parser = http_parser_create(HTTP_PARSER_REQUEST, NULL, NULL);
	assert(AF_INET == sa->sa_family || AF_INET6 == sa->sa_family);
	assert(salen <= sizeof(session->addr));
	memcpy(&session->addr, sa, salen);
	session->rlocker = session;
	session->addrlen = salen;
	session->socket = socket;
	session->param = server->param;
	session->handler = server->handler;
	session->transport = aio_tcp_transport_create(socket, &handler, session);
	if (0 != aio_tcp_transport_recv(session->transport, session->data, HTTP_RECV_BUFFER))
	{
		aio_tcp_transport_destroy(session->transport);
		return -1;
	}
	return 0;
}

int http_server_set_content_length(http_session_t* session, int64_t value)
{
	int len;
	char str[64];

	if (value < 0)
	{
		session->http_content_length_flag = 1; // don't need Content-Length
		return 0;
	}

	len = snprintf(str, sizeof(str), "%" PRIu64, value);
	return http_session_add_header(session, "Content-Length", str, len);
}

int http_server_set_status_code(http_session_t* session, int code, const char* status)
{
	int r;
	session->http_response_code_flag = 1;
	r = snprintf(session->status_line, sizeof(session->status_line)-1, "HTTP/1.1 %d %s\r\n", code, (status && *status) ? status : http_reason_phrase(code));
	return r >= sizeof(session->status_line)-1 ? -E2BIG : (r < 0 ? r : 0);
}

int http_server_send(struct http_session_t *session, const void* data, size_t bytes, http_server_onsend onsend, void* param)
{
	struct http_vec_t vec;
	vec.data = data;
	vec.bytes = bytes;
	return http_server_send_vec(session, &vec, 1, onsend, param);
}

int http_server_send_vec(struct http_session_t *session, const struct http_vec_t* vec, int num, http_server_onsend onsend, void* param)
{
	int r;
	char content_length[32];
	if (session->ws)
		return -1; // websocket can't use http reply mode

	r = http_session_data(session, vec, num, session->http_response_header_flag ? 0 : 3);
	if (r < 0) 
		return r;

	// HTTP Response Header, only one
	if (0 == session->http_response_header_flag)
	{
		session->http_response_header_flag = 1;

		// Content-Length
		if (0 == session->http_content_length_flag)
		{
			r = snprintf(content_length, sizeof(content_length), "%d", r);
			http_session_add_header(session, "Content-Length", content_length, r);
		}

		if(!session->http_response_code_flag)
			snprintf(session->status_line, sizeof(session->status_line), "HTTP/1.1 %d %s\r\n", 200, http_reason_phrase(200));
		socket_setbufvec(session->vec, 0, session->status_line, strlen(session->status_line));
		socket_setbufvec(session->vec, 1, session->header, session->header_size);
		socket_setbufvec(session->vec, 2, (void*)s_http_header_end, 2);
	}

	session->onsend = onsend;
	session->onsendparam = param;
	return aio_tcp_transport_send_v(session->transport, session->vec, session->vec_count);
}

static socket_bufvec_t* socket_bufvec_alloc(struct http_session_t *session, int count)
{
	void* p;
	if (count <= 5)
		return session->vec5;

	if (count > session->vec_capacity)
	{
		p = realloc(session->__vec, count * sizeof(socket_bufvec_t));
		if (NULL == p)
			return NULL;

		session->__vec = (socket_bufvec_t *)p;
		session->vec_capacity = count;
	}

	return session->__vec;
}

static int http_session_data(struct http_session_t *session, const struct http_vec_t* vec, int num, int reserved)
{
	int i;
	int len = 0;

	// multi-thread onrecv maybe before onsend
	assert(NULL == session->vec);
	assert(0 == session->vec_count);
	session->vec = socket_bufvec_alloc(session, num + reserved);
	if (!session->vec || num < 1)
		return -1;

	// HTTP Response Data
	for (i = 0; i < num; i++)
	{
		socket_setbufvec(session->vec, i + reserved, (void*)vec[i].data, vec[i].bytes);
		len += vec[i].bytes;
	}

	session->vec_count = num + reserved;
	return len;
}

int http_session_add_header(struct http_session_t* session, const char* name, const char* value, size_t bytes)
{
	void* ptr;
	size_t len;
	len = strlen(name ? name : "");
	if (!name || !value || len < 1 || bytes < 1)
		return -EINVAL;

	if (session->header_size + len + bytes + 4 > session->header_capacity)
	{
		if (session->header == (char*)(session + 1))
		{
			ptr = malloc(session->header_capacity + len + bytes + 2 * 1024);
			if (ptr)
				memcpy(ptr, session->header, session->header_size);
		}
		else
		{
			ptr = realloc(session->header, session->header_capacity + len + bytes + 2 * 1024);
		}

		if (!ptr)
			return -ENOMEM;
		session->header = ptr;
		session->header_capacity += len + bytes + 2 * 1024;
	}

	session->header_size += snprintf(session->header + session->header_size, session->header_capacity - session->header_size, "%s: %s\r\n", name, value ? value : "");
	
	// check http header
	if (0 == strcasecmp(name, "Transfer-Encoding") && 0 == strcasecmp("chunked", value))
	{
		session->http_transfer_encoding_chunked_flag = 1;
		session->http_content_length_flag = 1; // don't need Content-Length
	}
	else if (0 == strcasecmp(name, "Content-Length"))
	{
		session->http_content_length_flag = 1;
	}
	return 0;
}

//struct http_websocket_t* http_server_websocket_upgrade(http_session_t* session, struct websocket_handler_t* handler, void* param)
//{
//	struct http_websocket_t* ws;
//	ws = calloc(1, sizeof(*ws));
//	if (!ws)
//		return NULL;
//
//	session->ws = ws;
//	ws->session = session;
//	ws->param = param;
//	memcpy(&ws->handler, handler, sizeof(ws->handler));
//	return ws;
//}

int http_session_websocket_send_vec(struct http_websocket_t* ws, int opcode, const struct http_vec_t* vec, int num)
{
	int r;
	r = http_session_data(ws->session, vec, num, 1);
	if (r < 0)
		return r;

	// Frame header
	struct websocket_header_t wsh;
	memset(&wsh, 0, sizeof(wsh));
	wsh.fin = 1; // FIN
	wsh.len = r;
	wsh.opcode = opcode;
	r = websocket_header_write(&wsh, (uint8_t*)ws->session->status_line, sizeof(ws->session->status_line));
	assert(r > 0 && r <= 14); // WebSocket Frame Header max length

	socket_setbufvec(ws->session->vec, 0, ws->session->status_line, r);

	ws->session->onsend = ws->handler.onsend;
	ws->session->onsendparam = ws->param;
	return aio_tcp_transport_send_v(ws->session->transport, ws->session->vec, ws->session->vec_count);
}
