#include "sockutil.h"
#include "http-server.h"
#include "http-server-internal.h"
#include <stdlib.h>

int http_server_init(void)
{
	return 0;
}

int http_server_cleanup(void)
{
	return 0;
}

void* http_server_create(const char* ip, int port)
{
	socket_t socket;
	struct http_server_t *ctx;
	struct aio_tcp_transport_handler_t handler;

	// create server socket
	socket = socket_tcp_listen(ip, (u_short)port, SOMAXCONN);
	if(socket_invalid == socket)
	{
		printf("http_server_create(%s, %d): create socket error.\n", ip, port);
		return NULL;
	}

	ctx = (struct http_server_t*)calloc(1, sizeof(ctx[0]));
	if(!ctx)
		return NULL;

	handler.onconnected = http_session_onconnected;
	handler.ondisconnected = http_session_ondisconnected;
	handler.onrecv = http_session_onrecv;
	handler.onsend = http_session_onsend;
	ctx->transport = aio_tcp_transport_create(socket, &handler, ctx);
	if(!ctx->transport)
	{
		printf("http_server_create(%s, %d) create aio transport error.\n", ip, port);
		http_server_destroy(ctx);
		socket_close(socket);
		return NULL;
	}

	return ctx;
}

int http_server_destroy(void* http)
{
	struct http_server_t *ctx;
	ctx = (struct http_server_t*)http;

	if(ctx->transport)
		aio_tcp_transport_destroy(ctx->transport);

	free(ctx);
	return 0;
}

int http_server_set_handler(void* http, http_server_handler handler, void* param)
{
	struct http_server_t *ctx;
	ctx = (struct http_server_t*)http;
	ctx->handle = handler;
	ctx->param = param;
	return 0;
}
