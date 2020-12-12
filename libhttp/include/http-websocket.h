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
	int (*onsend)(void* param, int code, size_t bytes);

	void (*ondestroy)(void* param);

	/// Data Frames
	/// @param[in] opcode websocket message type, binary, text, ping, pong, close, etc...
	/// @param[in] data websocket data, after de-mask
	/// @param[in] flags websocket flags, WEBSOCKET_FLAGS_xxx
	/// @return 0-ok, other-close websocket
	int (*ondata)(void* param, int opcode, const void* data, size_t bytes, int flags);
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
