#include "http-server.h"
#include "http-websocket.h"
#include "http-server-internal.h"
#include "http-websocket-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int websocket_destory(struct http_websocket_t* ws)
{
	if (ws)
	{
		// TODO: close http session
		http_session_websocket_destroy(ws);

		//aio_tcp_transport_destroy(ws->session->transport);
		//
		//websocket_parser_destroy(&ws->parser);
		//free(ws);
	}
	return 0;
}

int websocket_send_ping(struct http_websocket_t* ws, const void* data, size_t bytes)
{
	return websocket_send(ws, WEBSOCKET_OPCODE_PING, data, bytes);
}

int websocket_send_pong(struct http_websocket_t* ws, const void* data, size_t bytes)
{
	return websocket_send(ws, WEBSOCKET_OPCODE_PONG, data, bytes);
}

int websocket_send_close(struct http_websocket_t* ws, const void* msg, size_t bytes)
{
	return websocket_send(ws, WEBSOCKET_OPCODE_CLOSE, msg, bytes);
}

int websocket_send(struct http_websocket_t* ws, int opcode, const void* data, size_t bytes)
{
	struct http_vec_t vec;
	vec.data = data;
	vec.bytes = bytes;
	return websocket_send_vec(ws, opcode, &vec, 1);
}

int websocket_send_vec(struct http_websocket_t* ws, int opcode, const struct http_vec_t* vec, int num)
{
	return http_session_websocket_send_vec(ws, opcode, vec, num);
}

int websocket_set_maxbufsize(struct http_websocket_t* ws, size_t bytes)
{
	if (bytes > 1 * 1024 * 1024 * 1024 || bytes < 64)
		return -1;

	ws->parser.max_capacity = (unsigned int)bytes;
	return 0;
}
