#ifndef _http_server_internal_h_
#define _http_server_internal_h_

#include "http-server.h"
#include "sys/sock.h"

struct http_session_t
{
	void* parser; // HTTP parser
	void* transport; // TCP transporot
	socklen_t addrlen;
	struct sockaddr_storage addr;

	size_t offset; // header offset/length
	char status_line[64];
	char header[2 * 1024];
	
	// send buffer vector
	int vec_count;
	int vec_capacity;
	socket_bufvec_t *vec;
	socket_bufvec_t vec4[4];
	socket_bufvec_t *__vec;

	http_server_handler handler;
	void* param;
};

struct http_server_t
{
	void* aio;

	http_server_handler handler;
	void* param;
};

int http_session_create(struct http_server_t *server, socket_t socket, const struct sockaddr* sa, socklen_t salen);

#endif /* !_http_server_internal_h_ */
