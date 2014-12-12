#include "http-client-connect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
#include "cstringext.h"
#include "http-client.h"
#include "sys/sock.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "aio-socket.h"
#include "http-parser.h"

struct http_pool_t;

struct http_connection_t
{
	volatile int32_t ref;
	volatile int32_t running;
	locker_t locker;

	void* parser; // http parser
	aio_socket_t socket;

	// http request header
	char *req;
	size_t nreq, maxreq;

	// user post data
	http_client_response callback;
	void* cbparam;
	const void* msg;
	size_t nmsg;
	socket_bufvec_t vec[2];
	size_t nsend;

	int initialize; // 0-uninitialize, 1-initialized

	struct http_pool_t *pool;
};

//////////////////////////////////////////////////////////////////////////
/// HTTP Connection API
static void http_connection_release(struct http_connection_t* http)
{
	assert(http->ref > 0);
	if(0 == atomic_decrement32(&http->ref))
	{
		assert(0 == http->ref);
		assert(0 == http->running);
		if(http->parser)
		{
			http_parser_destroy(http->parser);
			http->parser = NULL;
		}

		if(invalid_aio_socket != http->socket)
		{
			aio_socket_destroy(http->socket);
			http->socket = invalid_aio_socket;
		}

		locker_destroy(&http->locker);

#if defined(DEBUG) || defined(_DEBUG)
		memset(http, 0xCC, sizeof(*http));
#endif

		http_pool_release(http->pool);
	}
}

static void http_connection_destroy(struct http_connection_t *http)
{
	atomic_cas32(&http->running, 1, 0); // clear running flag

	locker_lock(&http->locker);
	if(invalid_aio_socket != http->socket)
	{
		aio_socket_destroy(http->socket);
		http->socket = invalid_aio_socket ;
	}

	http->initialize = 0;
	locker_unlock(&http->locker);

	http_connection_release(http);
}

static int http_connection_init(struct http_connection_t *http)
{
	if(http->initialize)
		return 0;

	locker_create(&http->locker);

	http->socket = invalid_aio_socket;

	http->parser = http_parser_create(HTTP_PARSER_CLIENT);
	if(!http->parser)
	{
		http_connection_release(http);
		return ENOMEM;
	}

	assert(!http->req && 0 == http->maxreq);
	http->maxreq = 1024;
	http->req = (char*)malloc(http->maxreq);
	if(!http->req)
	{
		http_connection_release(http);
		return ENOMEM;
	}

	http->initialize = 1; // set initialize flag
	return 0;
}

static void http_connection_action(struct http_connection_t *http, int code)
{
	locker_lock(&http->locker);
	if(http->initialize)
		http->callback(http->cbparam, http, code);
	locker_unlock(&http->locker);

	assert(1 == http->running);
	atomic_cas32(&http->running, 1, 0); // clear running flag
}

static int http_connection_connect(struct http_connection_t *http, const char* ip, int port);

static void http_connection_onrecv(void* param, int code, size_t bytes)
{
	int r;
	struct http_connection_t *http;
	http = (struct http_connection_t *)param;

	if(0 == code)
	{
		r = http_parser_input(http->parser, http->req, &bytes);
		if(1 == r)
		{
			// receive more data
			atomic_increment32(&http->ref);
			locker_lock(&http->locker);
			r = aio_socket_recv(http->socket, http->req, http->maxreq, http_connection_onrecv, http);
			locker_unlock(&http->locker);
			if(0 != r)
			{
				atomic_decrement32(&http->ref);
				http_connection_action(http, r); // aio socket error
			}
		}
		else
		{
			// ok or error
			http_connection_action(http, r);
		}
	}
	else
	{
		http_connection_action(http, code); // receive error
	}

	http_connection_release(http);
}

static void http_connection_onsend(void* param, int code, size_t bytes)
{
	int r;
	struct http_connection_t *http;
	http = (struct http_connection_t *)param;

	if(0 == code)
	{
		http->nsend += bytes;
		if(bytes == http->nreq + http->nmsg)
		{
			//http_parser_clear(http->parser);

			// receive reply
			atomic_increment32(&http->ref);
			locker_lock(&http->locker);
			r = aio_socket_recv(http->socket, http->req, http->maxreq, http_connection_onrecv, http);
			locker_unlock(&http->locker);
			if(0 != r)
			{
				atomic_decrement32(&http->ref);
				http_connection_action(http, r);  // aio socket error
			}
		}
		else
		{
			// send remain data
			assert(http->ref > 0);
			atomic_increment32(&http->ref);
			if(http->nsend < http->nreq)
			{
				socket_setbufvec(http->vec, 0, http->req+http->nsend, http->nreq-http->nsend);
				socket_setbufvec(http->vec, 1, (void*)http->msg, http->nmsg);
				locker_lock(&http->locker);
				r = aio_socket_send_v(http->socket, http->vec, 0==http->nmsg?1:2, http_connection_onsend, http);
				locker_unlock(&http->locker);
			}
			else
			{
				assert(http->nmsg > 0);
				locker_lock(&http->locker);
				r = aio_socket_send(http->socket, (char*)http->msg+(http->nsend-http->nreq), http->nmsg-(http->nsend-http->nreq), http_connection_onsend, http);
				locker_unlock(&http->locker);
			}

			if(0 != r)
			{
				atomic_decrement32(&http->ref);
				http_connection_action(http, r); // aio socket error
			}
		}
	}
	else
	{
		if(1 == http->running && 0==http->nsend)
		{
			// first write failed(maybe reset by peer).
			// try again...

			// destroy
			aio_socket_destroy(http->socket);
			http->socket = invalid_aio_socket;

			// try to connect
			code = http_connection_connect(http, http->pool->ip, http->pool->port);
		}

		if(0 != code)
		{
			http_connection_action(http, code);
		}
	}

	http_connection_release(http);
}

static void http_connection_onconnect(void* param, int code)
{
	struct http_connection_t *http;
	http = (struct http_connection_t *)param;
	if(0 != code)
	{
		aio_socket_destroy(http->socket);
		http->socket = invalid_aio_socket;
		//http->pool->status = -1; // can't connect

		http_connection_action(http, code);
	}
	else
	{
		http->running += 1; // send from onconnection
		atomic_increment32(&http->ref);
		locker_lock(&http->locker);
		code = aio_socket_send_v(http->socket, http->vec, 0==http->nmsg?1:2, http_connection_onsend, http);
		locker_unlock(&http->locker);
		if(0 != code)
		{
			atomic_decrement32(&http->ref);
			http_connection_action(http, code);
		}
	}

	http_connection_release(http);
}

static int http_connection_connect(struct http_connection_t *http, const char* ip, int port)
{
	int r = 0;
	socket_t socket;

	assert(invalid_aio_socket == http->socket);
	socket = socket_tcp();
#if defined(OS_WINDOWS)
	r = socket_bind_any(socket, 0);
	if(0 != r)
	{
		socket_close(socket);
		return r;
	}
#endif

	http->socket = aio_socket_create(socket, 1);
	if(!http->socket)
	{
		socket_close(socket);
		return -1;
	}

	atomic_increment32(&http->ref);
	locker_lock(&http->locker);
	r = aio_socket_connect(http->socket, ip, port, http_connection_onconnect, http);
	locker_unlock(&http->locker);
	if(0 != r)
	{
		atomic_decrement32(&http->ref);
		return r;
	}

	return 0;
}

static int http_connection_send(struct http_connection_t *http, const void* msg, size_t bytes)
{
	int r;

	http->nsend = 0; // clear sent bytes
	http->msg = msg;
	http->nmsg = bytes;
	socket_setbufvec(http->vec, 0, http->req+http->nsend, http->nreq-http->nsend);
	socket_setbufvec(http->vec, 1, (void*)http->msg, http->nmsg);

	if(invalid_aio_socket == http->socket)
	{
		r = http_connection_connect(http, http->pool->ip, http->pool->port);
	}
	else
	{
		atomic_increment32(&http->ref);
		r = aio_socket_send_v(http->socket, http->vec, 0==http->nmsg?1:2, http_connection_onsend, http);
		if(0 != r)
			atomic_decrement32(&http->ref);
	}
	return r;
}

struct http_client_connection_t* http_client_connection_aio()
{
	static struct http_client_connection_t conn = {
		http_create,
		http_destroy,
		http_request,
	}

	return &conn;
}
