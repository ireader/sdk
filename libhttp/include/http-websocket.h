#ifndef _http_websocket_h_
#define _http_websocket_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct http_websocket_t;

struct http_vec_t
{
	const void* data;
	size_t bytes;
};

enum
{
	WEBSOCKET_OPCODE_CONTINUATION	= 0,
	WEBSOCKET_OPCODE_TEXT			= 1,
	WEBSOCKET_OPCODE_BINARY			= 2,
	WEBSOCKET_OPCODE_CLOSE			= 8,
	WEBSOCKET_OPCODE_PING			= 9,
	WEBSOCKET_OPCODE_PONG			= 10,
};

enum
{
	WEBSOCKET_FLAGS_START	= 0x01,
	WEBSOCKET_FLAGS_FIN		= 0x02,
};

enum
{
	WEBSOCKET_TYPE_TEXT		= 0x01,
	WEBSOCKET_TYPE_MESSAGE	= 0x02,
};

struct websocket_handler_t
{
	/// WebSocket upgrade
	/// Notice: SHOULD Use http_server_set_header set websocket sub-protocol if subprotocols not empty
	/// @param[in] param http_server_websocket_handler parameter
	/// @param[in] path websocket request path
	/// @param[in] subprotocols client request subprotocols, maybe NULL
	/// @param[out] wsparam user-defined websocket session parameter
	/// @return 0-ok(accept with user-defined parameter), other-decline(socket can use again)
	int (*onupgrade)(void* param, struct http_websocket_t* ws, const char* path, const char* subprotocols, void** wsparam);

	/// @param[in] wsparam onupgrade return value
	void (*ondestroy)(void* wsparam);

	/// @param[in] wsparam onupgrade return value
	int (*onsend)(void* wsparam, int code, size_t bytes);

	/// On Receive WebSocket Frame
	/// @param[in] wsparam onupgrade return value
	/// @param[in] opcode <0-error, >=0-websocket message type, binary, text, ping, pong, close, etc...
	/// @param[in] data websocket data, after de-mask
	/// @param[in] flags websocket flags, WEBSOCKET_FLAGS_xxx
	/// @return 0-ok, other-close websocket
	int (*ondata)(void* wsparam, int opcode, const void* data, size_t bytes, int flags);
};

int websocket_destory(struct http_websocket_t* ws);

int websocket_send_ping(struct http_websocket_t* ws, const void* data, size_t bytes);

int websocket_send_pong(struct http_websocket_t* ws, const void* data, size_t bytes);

int websocket_send_close(struct http_websocket_t* ws, const void* msg, size_t bytes);

int websocket_send(struct http_websocket_t* ws, int opcode, const void* data, size_t bytes);

int websocket_send_vec(struct http_websocket_t* ws, int opcode, const struct http_vec_t* vec, int num);

int websocket_set_maxbufsize(struct http_websocket_t* ws, size_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_http_websocket_h_ */
