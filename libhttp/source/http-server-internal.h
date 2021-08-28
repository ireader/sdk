#ifndef _http_server_internal_h_
#define _http_server_internal_h_

#include "http-server.h"
#include "http-parser.h"
#include "http-websocket-internal.h"
#include "aio-transport.h"
#include "sys/sock.h"
#include "sys/atomic.h"

struct http_websocket_t
{
    struct websocket_parser_t parser;
};

struct http_server_t
{
	void* aio;

	http_server_handler handler;
	void* param;

	struct websocket_handler_t wshandler;
	void* wsparam;
};

struct http_session_t
{
	int32_t ref;
	http_parser_t* parser; // HTTP parser
	aio_transport_t* transport; // TCP transporot
	socket_t socket;
	socklen_t addrlen;
	struct sockaddr_storage addr;

	char* data; // recv buffer
	size_t remain; // remain size
	void* rlocker; // recv status
	char status_line[64];
	struct
	{
		char* ptr;
		size_t len;
		size_t cap;
	} header;

	int http_response_code_flag; // 0-don't set status code
	int http_response_header_flag; // 0-don't send response header, 1-sent
	int http_content_length_flag; // 0-calc length, 1-user input value
	int http_transfer_encoding_chunked_flag; // 0-bytes, 1-chunked
	
	// send buffer vector
	struct
	{
		int count;
		int capacity;
		socket_bufvec_t* vec;
		socket_bufvec_t vec12[12];
		socket_bufvec_t* __vec;
	} vec;

	// payload
	struct
	{
		char* ptr;
		size_t len;
		size_t cap;
		size_t max;
	} payload;

	struct http_server_t* server;
	struct http_websocket_t websocket;

	struct
	{
		struct http_streaming_handler_t handler;
		void* param;
	} streaming;

	http_server_onsend onsend;
	void* onsendparam;

	int tryupgrade;
	void* wsupgrade;
};

struct http_session_t* http_session_create(struct http_server_t *server, socket_t socket, const struct sockaddr* sa, socklen_t salen);

int http_session_add_header(struct http_session_t* session, const char* name, const char* value, size_t bytes);

int http_session_websocket_destroy(struct http_websocket_t* ws);

int http_session_websocket_send_vec(struct http_websocket_t* ws, int opcode, const struct http_vec_t* vec, int num);


#endif /* !_http_server_internal_h_ */
