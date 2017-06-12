#include "cstringext.h"
#include "sockutil.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "aio-socket.h"
#include "http-parser.h"
#include "http-client-connect.h"

struct http_client_aio_transport_t
{
	int32_t ref;
	locker_t locker;
	int running;
	aio_socket_t socket;
	void *parser;
	http_client_response callback;
	void* cbparam;

	char buffer[1024];

	const char* req;
	size_t nreq;
	const void* msg;
	size_t nmsg;
	socket_bufvec_t vec[2];
	size_t nsend;

	int retry;
	struct http_pool_t *pool;
};

static int http_connect(struct http_client_aio_transport_t *http, const char* ip, int port);

//////////////////////////////////////////////////////////////////////////
/// HTTP Connection API
static void http_release(struct http_client_aio_transport_t* http)
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

		assert(invalid_aio_socket == http->socket);
		//if(invalid_aio_socket != http->socket)
		//{
		//	aio_socket_destroy(http->socket);
		//	http->socket = invalid_aio_socket;
		//}

		locker_destroy(&http->locker);

		http_pool_release(http->pool);

#if defined(DEBUG) || defined(_DEBUG)
		memset(http, 0xCC, sizeof(*http));
#endif
		free(http);
	}
}

static void* http_create(struct http_pool_t *pool)
{
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)malloc(sizeof(*http));
	if(!http)
		return NULL;

	atomic_increment32(&pool->ref);
	memset(http, 0, sizeof(*http));
	http->ref = 1;
	http->pool = pool;
	http->running = 1;
	locker_create(&http->locker);
	http->socket = invalid_aio_socket;
	http->parser = http_parser_create(HTTP_PARSER_CLIENT);
	if(!http->parser)
	{
		http_release(http);
		return NULL;
	}
	return http;
}

static void http_destroy(void *p)
{
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)p;

	locker_lock(&http->locker);
	http->running = 0;
	if(invalid_aio_socket != http->socket)
	{
		aio_socket_destroy(http->socket);
		http->socket = invalid_aio_socket ;
	}
	locker_unlock(&http->locker);

	http_release(http);
}

static void http_onrecv(void* param, int code, size_t bytes)
{
	int r;
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)param;

	locker_lock(&http->locker);
	if(http->running)
	{
		if(0 == code)
		{
			r = http_parser_input(http->parser, http->buffer, &bytes);
			if(1 == r)
			{
				// receive more data
				atomic_increment32(&http->ref);
				r = aio_socket_recv(http->socket, http->buffer, sizeof(http->buffer), http_onrecv, http);
				if(0 != r)
				{
					atomic_decrement32(&http->ref);
					http->callback(http->cbparam, http->parser, r);
				}
			}
			else
			{
				// ok or error
				http->callback(http->cbparam, http->parser, r);
			}
		}
		else
		{
			http->callback(http->cbparam, http->parser, code); // receive error
		}
	}
	locker_unlock(&http->locker);

	http_release(http);
}

static void http_onsend(void* param, int code, size_t bytes)
{
	int r;
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)param;

	locker_lock(&http->locker);
	if(http->running)
	{
		if(0 == code)
		{
			http->nsend += bytes;
			if(bytes == http->nreq + http->nmsg)
			{
				//http_parser_clear(http->parser);

				// receive reply
				atomic_increment32(&http->ref);
				r = aio_socket_recv(http->socket, http->buffer, sizeof(http->buffer), http_onrecv, http);
				if(0 != r)
				{
					if(0 == http->retry++)
					{
						// read failed(maybe reset by peer).
						// try again...

						// destroy
						aio_socket_destroy(http->socket);
						http->socket = invalid_aio_socket;

						// try to connect
						r = http_connect(http, http->pool->ip, http->pool->port);
					}

					if(0 != r)
					{
						atomic_decrement32(&http->ref);
						http->callback(http->cbparam, http->parser, r);  // aio socket error
					}
				}
			}
			else
			{
				// send remain data
				assert(http->ref > 0);
				atomic_increment32(&http->ref);
				if(http->nsend < http->nreq)
				{
					socket_setbufvec(http->vec, 0, (char*)http->req+http->nsend, http->nreq-http->nsend);
					socket_setbufvec(http->vec, 1, (void*)http->msg, http->nmsg);
					r = aio_socket_send_v(http->socket, http->vec, 0==http->nmsg?1:2, http_onsend, http);
				}
				else
				{
					assert(http->nmsg > 0);
					r = aio_socket_send(http->socket, (char*)http->msg+(http->nsend-http->nreq), http->nmsg-(http->nsend-http->nreq), http_onsend, http);
				}

				if(0 != r)
				{
					atomic_decrement32(&http->ref);
					http->callback(http->cbparam, http->parser, r); // aio socket error
				}
			}
		}
		else
		{
			if(0 == http->retry++)
			{
				// first write failed(maybe reset by peer).
				// try again...

				// destroy
				aio_socket_destroy(http->socket);
				http->socket = invalid_aio_socket;

				// try to connect
				code = http_connect(http, http->pool->ip, http->pool->port);
			}

			if(0 != code)
			{
				http->callback(http->cbparam, http->parser, code);
			}
		}
	}
	locker_unlock(&http->locker);

	http_release(http);
}

static void http_onconnect(void* param, int code)
{
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)param;

	locker_lock(&http->locker);
	if(http->running)
	{
		if(0 != code)
		{
			if(http->socket)
			{
				aio_socket_destroy(http->socket);
				http->socket = invalid_aio_socket;
			}
			http->callback(http->cbparam, http->parser, code);
		}
		else
		{
			atomic_increment32(&http->ref);
			code = aio_socket_send_v(http->socket, http->vec, 0==http->nmsg?1:2, http_onsend, http);
			if(0 != code)
			{
				atomic_decrement32(&http->ref);
				http->callback(http->cbparam, http->parser, code);
			}
		}
	}
	locker_unlock(&http->locker);

	http_release(http);
}

static int http_connect(struct http_client_aio_transport_t *http, const char* ip, int port)
{
	int r = 0;
	socket_t tcp;
	socklen_t addrlen;
	struct sockaddr_storage ss;

	assert(http->running);
	assert(invalid_aio_socket == http->socket);
	r = socket_addr_from(&ss, &addrlen, ip, (u_short)port);
	if (0 != r)
		return r;

	assert(ss.ss_family == AF_INET || ss.ss_family == AF_INET6);
	tcp = socket(ss.ss_family, SOCK_STREAM, 0);
#if defined(OS_WINDOWS)
	r = socket_bind_any(tcp, 0);
	if(0 != r)
	{
		socket_close(tcp);
		return r;
	}
#endif

	http->socket = aio_socket_create(tcp, 1);
	if(!http->socket)
	{
		socket_close(tcp);
		return -1;
	}

	atomic_increment32(&http->ref);
	r = aio_socket_connect(http->socket, (struct sockaddr*)&ss, addrlen, http_onconnect, http);
	if(0 != r)
	{
		atomic_decrement32(&http->ref);
		aio_socket_destroy(http->socket);
		http->socket = invalid_aio_socket;
		return r;
	}

	return 0;
}

static int http_request(void *conn, const char* req, size_t nreq, const void* msg, size_t bytes, http_client_response callback, void *param)
{
	int r;
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)conn;
	http_parser_clear(http->parser); // clear http parser status
	http->callback = callback;
	http->cbparam = param;
	http->nsend = 0; // clear sent bytes
	http->req = req;
	http->nreq = nreq;
	http->msg = msg;
	http->nmsg = bytes;
	http->retry = 0; // clear retry flag

	socket_setbufvec(http->vec, 0, (char*)http->req+http->nsend, http->nreq-http->nsend);
	socket_setbufvec(http->vec, 1, (void*)http->msg, http->nmsg);

	locker_lock(&http->locker);
	if(invalid_aio_socket == http->socket)
	{
		r = http_connect(http, http->pool->ip, http->pool->port);
	}
	else
	{
		atomic_increment32(&http->ref);
		r = aio_socket_send_v(http->socket, http->vec, 0==http->nmsg?1:2, http_onsend, http);
		if(0 != r)
			atomic_decrement32(&http->ref);
	}
	locker_unlock(&http->locker);
	return r;
}

struct http_client_connection_t* http_client_connection_aio()
{
	static struct http_client_connection_t conn = {
		http_create,
		http_destroy,
		http_request,
	};
	return &conn;
}
