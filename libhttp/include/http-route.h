#ifndef _http_route_h_
#define _http_route_h_

#include "http-server.h"

#if defined(__cplusplus)
extern "C" {
#endif
	
int http_server_route(void* http, http_session_t* session, const char* method, const char* path);

int http_server_addroute(const char* path, http_server_handler handler);
int http_server_delroute(const char* path);

int http_server_reply(http_session_t* session, int code, const void* data, size_t bytes);

/// WebSocket

/// Notice: SHOULD Use http_server_set_header set websocket sub-protocol if subprotocols not empty
/// @param[in] path websocket request path
/// @param[in] subprotocols client request subprotocols, maybe NULL
/// @return 0-decline(socket can use again), other-ws param + accept websocket upgrade
typedef void* (*http_server_onupgrade)(void* param, http_websocket_t* ws, const char* path, const char* subprotocols);

/// not thread safe
void http_server_websocket_sethandler(struct websocket_handler_t* wsh, http_server_onupgrade handler, void* param);

#if defined(__cplusplus)
}
#endif
#endif /* !_http_route_h_ */
