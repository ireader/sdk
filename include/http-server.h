#ifndef _http_server_h_
#define _http_server_h_

#include "sys/sock.h"

void* http_server_create(socket_t sock);
int http_server_destroy(void **server);
void http_server_set_timeout(void *server, int recv, int send);
void http_server_get_timeout(void *server, int *recv, int *send);

int http_server_recv(void *server);
const char* http_server_get_path(void *server);
const char* http_server_get_method(void *server);
const char* http_server_get_header(void *server, const char *name);
int http_server_get_content(void *server, void **content, int *length);

int http_server_send(void* server, int code, const void* data, int bytes);
int http_server_set_header(void *server, const char* name, const char* value);
int http_server_set_header_int(void *server, const char* name, int value);

#endif /* !_http_server_h_ */
