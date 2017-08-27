#include "http-server.h"
#include "http-parser.h"
#include "http-server-internal.h"
#include "aio-tcp-transport.h"
#include "aio-accept.h"
#include "sockutil.h"
#include <stdlib.h>
#include <assert.h>

#define CONTENT_LENGTH_LEN 32

#if defined(OS_WINDOWS)
#define strcasecmp	_stricmp
#endif

static void http_server_onaccept(void* param, int code, socket_t socket, const struct sockaddr* addr, socklen_t addrlen)
{
	struct http_server_t *server;
	server = (struct http_server_t *)param;

	if (0 == code)
	{
		http_session_create(server, socket, addr, addrlen);
	}
	else
	{
		printf("http_server_onaccept code: %d\n", code);
	}
}

static int http_server_listen(struct http_server_t *server, const char* ip, int port)
{
	socket_t socket;

	// create server socket
	socket = socket_tcp_listen(ip, (u_short)port, SOMAXCONN);
	if (socket_invalid == socket)
	{
		printf("http_server_create(%s, %d): create socket error.\n", ip, port);
		return -1;
	}

	server->aio = aio_accept_start(socket, http_server_onaccept, server);
	if (NULL == server->aio)
	{
		printf("http_server_create(%s, %d): start accept error.\n", ip, port);
		socket_close(socket);
		return -1;
	}

	return 0;
}

static void http_server_ondestroy(void* param)
{
	struct http_server_t *server;
	server = (struct http_server_t *)param;
	free(server);
}

void* http_server_create(const char* ip, int port)
{
	struct http_server_t *server;

	server = (struct http_server_t*)calloc(1, sizeof(*server));
	if (server)
	{
		if (0 != http_server_listen(server, ip, port))
		{
			http_server_ondestroy(server);
			return NULL;
		}
	}

	return server;
}

int http_server_destroy(void* http)
{
	struct http_server_t *server;
	server = (struct http_server_t*)http;
	return aio_accept_stop(server->aio, http_server_ondestroy, server);
}

int http_server_set_handler(void* http, http_server_handler handler, void* param)
{
	struct http_server_t *ctx;
	ctx = (struct http_server_t*)http;
	ctx->handler = handler;
	ctx->param = param;
	return 0;
}

// Request
int http_server_get_client(struct http_session_t *session, char ip[65], unsigned short *port)
{
	if (NULL == ip || NULL == port)
		return -1;
	return socket_addr_to((struct sockaddr*)&session->addr, session->addrlen, ip, port);
}

const char* http_server_get_header(struct http_session_t *session, const char *name)
{
	return http_get_header_by_name(session->parser, name);
}

int http_server_get_content(struct http_session_t *session, void **content, size_t *length)
{
	*content = (void*)http_get_content(session->parser);
	*length = http_get_content_length(session->parser);
	return 0;
}

int http_server_set_header(struct http_session_t *session, const char* name, const char* value)
{
	assert(0 != strcasecmp("Content-Length", name));
	session->offset += snprintf(session->header + session->offset, sizeof(session->header) - session->offset - CONTENT_LENGTH_LEN, "%s: %s\r\n", name, value);
	return (session->offset + CONTENT_LENGTH_LEN < sizeof(session->header)) ? 0 : ENOMEM;
}

int http_server_set_header_int(struct http_session_t *session, const char* name, int value)
{
	assert(0 != strcasecmp("Content-Length", name));
	session->offset += snprintf(session->header + session->offset, sizeof(session->header) - session->offset - CONTENT_LENGTH_LEN, "%s: %d\r\n", name, value);
	return (session->offset + CONTENT_LENGTH_LEN < sizeof(session->header)) ? 0 : ENOMEM;
}

int http_server_set_content_type(struct http_session_t *session, const char* value)
{
	//Content-Type: application/json
	//Content-Type: text/html; charset=utf-8
	return http_server_set_header(session, "Content-Type", value);
}
