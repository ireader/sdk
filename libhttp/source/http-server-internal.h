#ifndef _http_server_internal_h_
#define _http_server_internal_h_

#include "http-server.h"
#include "http-parser.h"
#include "aio-tcp-transport.h"
#include "sys/sock.h"
#include "sys/atomic.h"

struct http_session_t
{
	http_parser_t* parser; // HTTP parser
	aio_tcp_transport_t* transport; // TCP transporot
	socket_t socket;
	socklen_t addrlen;
	struct sockaddr_storage addr;

	char* data; // recv buffer
	size_t remain; // remain size
	void* rlocker; // recv status

	char status_line[64];
	char* header;
	size_t header_capacity;
	size_t header_size;

	int http_content_length_flag; // 0-calc length, 1-user input value
	int http_transfer_encoding_flag; // 0-bytes, 1-chunked
	
	// send buffer vector
	int vec_count;
	int vec_capacity;
	socket_bufvec_t *vec;
	socket_bufvec_t vec5[5];
	socket_bufvec_t *__vec;

	http_server_handler handler;
	void* param;

	http_server_onsend onsend;
	void* onsendparam;
};

struct http_server_t
{
	void* aio;

	http_server_handler handler;
	void* param;
};

int http_session_create(struct http_server_t *server, socket_t socket, const struct sockaddr* sa, socklen_t salen);

int http_session_add_header(struct http_session_t* session, const char* name, const char* value, size_t bytes);

#endif /* !_http_server_internal_h_ */
