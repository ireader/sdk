#ifndef _http_client_connection_h_
#define _http_client_connection_h_

#define MAX_HTTP_CONNECTION 5

#include "sys/atomic.h"
#include "sys/locker.h"
#include "http-client.h"
#include "time64.h"

struct http_pool_t;

struct http_conn_t
{
	void* http;
	void* request;
	time64_t clock;

	struct http_pool_t *pool;
	http_client_response callback;
	void *cbparam;

#if defined(DEBUG) || defined(_DEBUG)
	int32_t count;
#endif
};

struct http_pool_t
{
	volatile int32_t ref;
	locker_t locker;

	char ip[128];
	unsigned short port;

	int rtimeout;
	int wtimeout;
	char *cookie;
	size_t ncookie;

	struct http_client_connection_t* api;
	struct http_conn_t https[MAX_HTTP_CONNECTION];
};

struct http_client_connection_t
{
	void* (*create)(struct http_pool_t *pool);
	void (*destroy)(void *http);

	int (*request)(void *conn, const char* req, size_t nreq, const void* msg, size_t bytes, http_client_response callback, void *param);
};

struct http_client_connection_t* http_client_connection_aio();
struct http_client_connection_t* http_client_connection_poll();

void http_pool_release(struct http_pool_t *pool);

#endif /* !_http_client_connection_h_ */
