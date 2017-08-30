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

#if defined(__cplusplus)
}
#endif
#endif /* !_http_route_h_ */
