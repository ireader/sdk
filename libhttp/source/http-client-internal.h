#ifndef _http_client_internal_h_
#define _http_client_internal_h_

#include "http-client.h"
#include "sys/sock.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "http-parser.h"
#include "http-request.h"

struct http_client_t
{
	http_client_onreply onreply;
	void *cbparam;

	volatile int32_t ref;
	locker_t locker;

	char host[128];
	unsigned short port;
	socket_t socket;

	struct {
		int conn;
		int recv;
		int send;
	} timeout;

	char *cookie;
	size_t ncookie;
	http_parser_t* parser;
	void* req;

	void* connection;
	struct http_client_connection_t* conn;
};

struct http_client_connection_t
{
	void* (*create)(struct http_client_t *http);
	void (*destroy)(struct http_client_t *http);
	void (*timeout)(struct http_client_t *http, int conn, int recv, int send);
	
	int (*request)(struct http_client_t *http, const char* req, size_t nreq, const void* msg, size_t bytes);
};

struct http_client_connection_t* http_client_connection(void);
struct http_client_connection_t* http_client_connection_aio(void);

void http_client_release(struct http_client_t* http);
void http_client_handle(struct http_client_t *http, int code);

#endif /* !_http_client_internal_h_ */
