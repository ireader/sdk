#ifndef _http_server_internal_h_
#define _http_server_internal_h_

#include "aio-tcp-transport.h"
#include "http-server.h"
#include "sys/atomic.h"
#include "sys/locker.h"

struct http_session_t
{
    int32_t ref;
    locker_t locker;
	struct http_server_t *server;

	void* session;
	void* parser;
	struct sockaddr_storage addr;
	socklen_t addrlen;

    char status_line[64];
	char header[2 * 1024];
	size_t offset; // header offset/length

	// send buffer vector
	int vec_count;
	socket_bufvec_t *vec;
	socket_bufvec_t vec3[3];
};

struct http_server_t
{
	void* transport;

	http_server_handler handle;
	void* param;
};

void* http_session_onconnected(void* ptr, void* session, const struct sockaddr* sa, socklen_t salen);
void http_session_ondisconnected(void* param);
int http_session_onsend(void* param, int code, size_t bytes);
int http_session_onrecv(void* param, const void* msg, size_t bytes);

#endif /* !_http_server_internal_h_ */
