#include "http-server-internal.h"
#include "aio-transport.h"
#include "http-reason.h"
#include "http-parser.h"
#include "sha.h"
#include "base64.h"
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

#define HTTP_RECV_BUFFER		(2*1024)
#define HTTP_HEADER_CAPACITY	(2*1024)
#define HTTP_PAYLOAD_LENGTH_MAX	(16*1024)

static socket_bufvec_t* socket_bufvec_alloc(struct http_session_t *session, int count);
static int http_session_data(struct http_session_t *session, const struct http_vec_t* vec, int num, int reserved);

static const char* s_http_header_end = "\r\n";

static int http_session_release(http_session_t* session)
{
	if (0 != atomic_decrement32(&session->ref))
		return 0;

	websocket_parser_destroy(&session->websocket.parser);

	if (session->parser)
	{
		http_parser_destroy(session->parser);
		session->parser = NULL;
	}

	if (session->vec.__vec)
	{
		free(session->vec.__vec);
		session->vec.__vec = NULL;
		session->vec.capacity = 0;
	}

	if (session->header.ptr != (char*)(session + 1))
	{
		assert(session->header.cap > 2 * 1024);
		free(session->header.ptr);
		session->header.ptr = NULL;
		session->header.len = 0;
		session->header.cap = 0;
	}

	if (session->payload.ptr)
	{
		free(session->payload.ptr);
		session->payload.ptr = NULL;
		session->payload.len = 0;
		session->payload.cap = 0;
	}

#if defined(DEBUG) || defined(_DEBUG)
	memset(session, 0xCC, sizeof(*session));
#endif
	free(session);
	return 0;
}

static void http_session_ondestroy(void* param)
{
	struct http_session_t *session;
	session = (struct http_session_t *)param;
	
	if (session->streaming.handler.ondestroy)
		session->streaming.handler.ondestroy(session->streaming.param);
	else if (session->wsupgrade && session->server->wshandler.ondestroy)
		session->server->wshandler.ondestroy(session->wsupgrade);

	http_session_release(session);
}

static void http_session_reset(struct http_session_t *session)
{
	session->header.len = 0;
	session->http_response_code_flag = 0;
	session->http_response_header_flag = 0;
	session->http_content_length_flag = 0;
	session->http_transfer_encoding_chunked_flag = 0;
	memset(&session->streaming, 0, sizeof(session->streaming));
	session->payload.len = 0; // clear content
	session->tryupgrade = 0;
}

static int http_session_try_upgrade(http_session_t* session)
{
	const char* upgrade;
	const char* connection;
	const char* wskey;
	const char* version;

	if (!session->server->wshandler.onupgrade || 0 != strcasecmp(http_get_request_method(session->parser), "GET"))
		return 0;

	upgrade = http_server_get_header(session, "Upgrade");
	if (!upgrade || 0 != strcasecmp(upgrade, "websocket"))
		return 0;

	connection = http_server_get_header(session, "Connection"); // keep-alive, Upgrade
	if (!connection || !strstr(connection, "Upgrade"))
		return 0;

	wskey = http_server_get_header(session, "Sec-WebSocket-Key");
	version = http_server_get_header(session, "Sec-WebSocket-Version");
	//const char* protocols = http_server_get_header(session, "Sec-WebSocket-Protocol");
	//const char* extensions = http_server_get_header(session, "Sec-WebSocket-Extensions");
	return wskey && version && 0 == strcasecmp(version, "13") ? 1 : 0;
}

static int http_session_upgrade_websocket(http_session_t* session, const char* path, const char* wskey, const char* protocols)
{
	static const char* wsuuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	uint8_t sha1[SHA1HashSize];
	char wsaccept[64] = { 0 };
	SHA1Context ctx;

	(void)protocols, (void)path;
	session->tryupgrade = 1;
	//session->wsupgrade = session->server->wshandler.onupgrade(session->server->wsparam, &session->websocket, path, protocols);
	//if (!session->wsupgrade)
	//{
	//	http_server_set_status_code(session, 401, NULL);
	//	return http_server_send(session, "", 0, NULL, NULL);
	//}
	//session->onsend = session->server->wshandler.onsend;
	//session->onsendparam = session->wsupgrade;

	SHA1Reset(&ctx);
	SHA1Input(&ctx, (const uint8_t*)wskey, (unsigned int)strlen(wskey));
	SHA1Input(&ctx, (const uint8_t*)wsuuid, (unsigned int)strlen(wsuuid));
	SHA1Result(&ctx, sha1);
	base64_encode(wsaccept, sha1, sizeof(sha1));

	http_server_set_header(session, "Upgrade", "WebSocket");
	http_server_set_header(session, "Connection", "Upgrade");
	http_server_set_header(session, "Sec-WebSocket-Accept", wsaccept);
	http_server_set_status_code(session, 101, NULL);
	return http_server_send(session, "", 0, NULL, NULL); // fix me
}

static void http_session_onrecv(void* param, int code, size_t bytes)
{
	int64_t len;
	struct http_session_t *session;
	session = (struct http_session_t *)param;
	session->remain = 0 == code ? (int)bytes : 0;

	// websocket
	if (session->wsupgrade)
	{
		code = websocket_parser_input(&session->websocket.parser, (uint8_t*)session->data, bytes, session->server->wshandler.ondata, session->wsupgrade);
		if (0 == code)
		{
			// recv more data
			code = aio_transport_recv(session->transport, session->data, HTTP_RECV_BUFFER);
			if (0 != code)
			{
				session->server->wshandler.ondata(session->wsupgrade, code > 0 ? -code : code, NULL, 0, 0);
			}
		}

		return;
	}
	
	// http
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

			if (http_session_try_upgrade(session))
			{
				const char* wskey = http_server_get_header(session, "Sec-WebSocket-Key");
				const char* protocols = http_server_get_header(session, "Sec-WebSocket-Protocol");
				//const char* extensions = http_server_get_header(session, "Sec-WebSocket-Extensions");
				code = http_session_upgrade_websocket(session, http_get_request_uri(session->parser), wskey, protocols);
			}
			else if (!session->streaming.handler.onrecv) // http callback once only
			{
				// call
				// user must reply(send/send_vec/send_file) in handle
				if (session->server->handler)
				{
					const char* uri = http_get_request_uri(session->parser);
					const char* method = http_get_request_method(session->parser);
					session->server->handler(session->server->param, session, method, uri);
				}
			}
		}
		else if (2 == code)
		{
			// header done
			assert(0 == session->remain);

			// fixme: once only
			if (!session->streaming.handler.onrecv && (0 != http_get_header_by_name2(session->parser, "Content-Length", &len) || len < 0 || len > HTTP_PAYLOAD_LENGTH_MAX))
			{
				len = session->payload.len; // save
				// clear for save user-defined header
				http_session_reset(session);

				// call
				// user must reply(send/send_vec/send_file) in handle
				if (session->server->handler)
				{
					const char* uri = http_get_request_uri(session->parser);
					const char* method = http_get_request_method(session->parser);
					session->server->handler(session->server->param, session, method, uri);
				}

				if (!session->streaming.handler.onrecv)
				{
					assert(0);
					code = -1; // don't support http streaming
				}
				else if (len > 0)
				{
					session->streaming.handler.onrecv(session->streaming.param, 0, session->payload.ptr, len);
				}
			}
		}

		if (code > 0)
		{
			assert(session->rlocker == session);

			// recv more data
			code = aio_transport_recv(session->transport, session->data, HTTP_RECV_BUFFER);
		}
	}

	// error or peer closed
	if(0 != code || 0 == bytes)
	{
		code = aio_transport_destroy(session->transport);
	}	
}

static void http_session_onsend(void* param, int code, size_t bytes)
{
	int r = 0;
	struct http_session_t *session;
	session = (struct http_session_t*)param;
	session->vec.count = 0;
	session->vec.vec = NULL;

	if (session->wsupgrade)
	{
		session->server->wshandler.onsend(session->wsupgrade, code, bytes);
		return;
	}
	
	// fixme : ignore websocket upgrade response
	if (session->tryupgrade)
	{
		const char* path = http_get_request_uri(session->parser);
		const char* protocols = http_server_get_header(session, "Sec-WebSocket-Protocol");

		session->tryupgrade = 0; // clear flags
		code = session->server->wshandler.onupgrade(session->server->wsparam, &session->websocket, path, protocols, &session->wsupgrade);
		if (0 == code)
		{
			// start recv websocket data
			code = aio_transport_recv(session->transport, session->data, HTTP_RECV_BUFFER);
			if (0 != code)
			{
				session->server->wshandler.ondata(session->wsupgrade, code > 0 ? -code : code, NULL, 0, 0);
			}
			return; // websocket mode
		}
	}
	else if (session->onsend)
	{
		r = session->onsend(session->onsendparam, code, bytes);
		if (0 == code && 1 == r)
			return; // HACK: has more data to send(such as sendfile)
	}

	if (0 == r && 0 == code && NULL == session->wsupgrade /* exclude websocket mode */)
	{
		http_parser_clear(session->parser); // reset parser
		if (session->remain > 0)
			http_session_onrecv(session, 0, session->remain); // next round
		else if (atomic_cas_ptr(&session->rlocker, NULL, session))
			r = aio_transport_recv(session->transport, session->data, HTTP_RECV_BUFFER);
	}

	if (0 != code || 0 != r)
	{
		code = aio_transport_destroy(session->transport);
	}
}

static void http_session_onhttp(void* param, const void* data, int len)
{
	void* p;
	struct http_session_t* session;
	session = (struct http_session_t*)param;

	if (session->streaming.handler.onrecv)
	{
		session->streaming.handler.onrecv(session->streaming.param, 0, data, len);
		return;
	}

	if (session->payload.len + (size_t)len + 1 /*filling zero*/ > session->payload.cap)
	{
		if (session->payload.len + (size_t)len > session->payload.max)
			return;

		p = realloc(session->payload.ptr, session->payload.len + (size_t)len + 1 + HTTP_RECV_BUFFER);
		if (!p)
			return;

		session->payload.ptr = p;
		session->payload.cap = session->payload.len + (size_t)len + HTTP_RECV_BUFFER;
	}

	memcpy(session->payload.ptr + session->payload.len, data, len);
	session->payload.len += (size_t)len;
	session->payload.ptr[session->payload.len] = 0; /*filling zero*/
}

struct http_session_t* http_session_create(struct http_server_t *server, socket_t socket, const struct sockaddr* sa, socklen_t salen)
{
	struct http_session_t *session;
	struct aio_transport_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.ondestroy = http_session_ondestroy;
	handler.onrecv = http_session_onrecv;
	handler.onsend = http_session_onsend;

	session = (struct http_session_t *)malloc(sizeof(*session) + HTTP_HEADER_CAPACITY + HTTP_RECV_BUFFER);
	if (!session) return NULL;

	memset(session, 0, sizeof(*session));
	session->ref = 1;
	session->header.ptr = (char*)(session + 1);
	session->header.cap = HTTP_HEADER_CAPACITY;
	session->payload.max = HTTP_PAYLOAD_LENGTH_MAX;
	session->data = session->header.ptr + session->header.cap;
	session->parser = http_parser_create(HTTP_PARSER_REQUEST, http_session_onhttp, session);
	assert(AF_INET == sa->sa_family || AF_INET6 == sa->sa_family);
	assert(salen <= sizeof(session->addr));
	memcpy(&session->addr, sa, salen);
	session->rlocker = session;
	session->server = server; // fixme: server->addref()
	session->addrlen = salen;
	session->socket = socket;
	session->transport = aio_transport_create(socket, &handler, session);
	if (0 != aio_transport_recv(session->transport, session->data, HTTP_RECV_BUFFER))
	{
		aio_transport_destroy(session->transport);
		return NULL;
	}
	return session;
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

	len = snprintf(str, sizeof(str), "%" PRId64, value);
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

	assert(!session->wsupgrade); // websocket can't use http reply mode
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
		socket_setbufvec(session->vec.vec, 0, session->status_line, strlen(session->status_line));
		socket_setbufvec(session->vec.vec, 1, session->header.ptr, session->header.len);
		socket_setbufvec(session->vec.vec, 2, (void*)s_http_header_end, 2);
	}

	session->onsend = onsend;
	session->onsendparam = param;
	return aio_transport_send_v(session->transport, session->vec.vec, session->vec.count);
}

static socket_bufvec_t* socket_bufvec_alloc(struct http_session_t *session, int count)
{
	void* p;
	if (count <= 12)
		return session->vec.vec12;

	if (count > session->vec.capacity)
	{
		p = realloc(session->vec.__vec, count * sizeof(socket_bufvec_t));
		if (NULL == p)
			return NULL;

		session->vec.__vec = (socket_bufvec_t *)p;
		session->vec.capacity = count;
	}

	return session->vec.__vec;
}

static int http_session_data(struct http_session_t *session, const struct http_vec_t* vec, int num, int reserved)
{
	int i;
	int len = 0;

	// multi-thread onrecv maybe before onsend
	assert(NULL == session->vec.vec);
	assert(0 == session->vec.count);
	session->vec.vec = socket_bufvec_alloc(session, num + reserved);
	if (!session->vec.vec || num < 1)
		return -1;

	// HTTP Response Data
	for (i = 0; i < num; i++)
	{
		socket_setbufvec(session->vec.vec, i + reserved, (void*)vec[i].data, vec[i].bytes);
		len += vec[i].bytes;
	}

	session->vec.count = num + reserved;
	return len;
}

int http_session_add_header(struct http_session_t* session, const char* name, const char* value, size_t bytes)
{
	void* ptr;
	size_t len;
	len = strlen(name ? name : "");
	if (!name || !value || len < 1 || bytes < 1)
		return -EINVAL;

	if (session->header.len + len + bytes + 4 > session->header.cap)
	{
		if (session->header.ptr == (char*)(session + 1))
		{
			ptr = malloc(session->header.cap + len + bytes + 2 * 1024);
			if (ptr)
				memcpy(ptr, session->header.ptr, session->header.len);
		}
		else
		{
			ptr = realloc(session->header.ptr, session->header.cap + len + bytes + 2 * 1024);
		}

		if (!ptr)
			return -ENOMEM;
		session->header.ptr = ptr;
		session->header.cap += len + bytes + 2 * 1024;
	}

	session->header.len += snprintf(session->header.ptr + session->header.len, session->header.cap - session->header.len, "%s: %s\r\n", name, value ? value : "");
	
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

void http_session_streaming_handler(http_session_t* session, struct http_streaming_handler_t* handler, void* param)
{
	memcpy(&session->streaming.handler, handler, sizeof(session->streaming.handler));
	session->streaming.param = param;
}

int http_session_close(http_session_t* session)
{
	return aio_transport_destroy(session->transport);
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
	struct http_session_t* session;
	session = ((struct http_session_t*)((char*)(ws)-(ptrdiff_t)(&((struct http_session_t*)0)->websocket)));

	r = http_session_data(session, vec, num, 1);
	if (r < 0)
		return r;

	// Frame header
	struct websocket_header_t wsh;
	memset(&wsh, 0, sizeof(wsh));
	wsh.fin = 1; // FIN
	wsh.len = r;
	wsh.opcode = opcode;
	r = websocket_header_write(&wsh, (uint8_t*)session->status_line, sizeof(session->status_line));
	assert(r > 0 && r <= 14); // WebSocket Frame Header max length

	socket_setbufvec(session->vec.vec, 0, session->status_line, r);
	return aio_transport_send_v(session->transport, session->vec.vec, session->vec.count);
}

int http_session_websocket_destroy(struct http_websocket_t* ws)
{
	struct http_session_t* session;
	session = ((struct http_session_t*)((char*)(ws)-(ptrdiff_t)(&((struct http_session_t*)0)->websocket)));
	return http_session_close(session);
}
