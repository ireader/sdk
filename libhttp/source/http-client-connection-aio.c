#include "sockutil.h"
#include "aio-socket.h"
#include "aio-rwutil.h"
#include "http-parser.h"
#include "http-client-connect.h"
#include <stdlib.h>

struct http_client_aio_transport_t
{
	aio_socket_t socket;
	void *parser;

	http_client_response callback;
	void* cbparam;

	int count;
	socket_bufvec_t vec[2];
	struct aio_socket_rw_t write;
	char buffer[1024];

	int retry;
	struct http_pool_t *pool;
};

//////////////////////////////////////////////////////////////////////////
/// HTTP Connection API

static void http_ondestroy(void* param)
{
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)param;

	if (http->parser)
	{
		http_parser_destroy(http->parser);
		http->parser = NULL;
	}

	http_pool_release(http->pool);

#if defined(DEBUG) || defined(_DEBUG)
	memset(http, 0xCC, sizeof(*http));
#endif
	free(http);
}

static void* http_create(struct http_pool_t *pool)
{
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)calloc(1, sizeof(*http));
	if(!http)
		return NULL;

	atomic_increment32(&pool->ref);
	http->pool = pool;
	http->socket = invalid_aio_socket;
	http->parser = http_parser_create(HTTP_PARSER_CLIENT);
	if(!http->parser)
	{
		http_ondestroy(http);
		return NULL;
	}
	return http;
}

static void http_destroy(void *p)
{
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)p;
	if (invalid_aio_socket != http->socket)
		aio_socket_destroy(http->socket, http_ondestroy, http);
	else
		http_ondestroy(http);
}

static void http_onrecv(void* param, int code, size_t bytes)
{
	int r;
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)param;

	if(0 == code)
	{
		r = http_parser_input(http->parser, http->buffer, &bytes);
		if(1 == r)
		{
			// receive more data
			code = aio_socket_recv(http->socket, http->buffer, sizeof(http->buffer), http_onrecv, http);
		}
		else
		{
			// receive done
			http->callback(http->cbparam, http->parser, r);
		}
	}
		
	if(0 != code)
	{
		http->callback(http->cbparam, http->parser, code); // receive error
	}
}

static void http_onsend(void* param, int code, size_t bytes)
{
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)param;

	(void)bytes;
	if(0 == code)
	{
		// receive reply
		code = aio_socket_recv(http->socket, http->buffer, sizeof(http->buffer), http_onrecv, http);
	}

	if(0 != code)
	{
		http->callback(http->cbparam, http->parser, code);
	}
}

static void http_onconnect(void* param, int code)
{
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)param;

	if (0 != code)
	{
		http->callback(http->cbparam, http->parser, code);
	}
	else
	{
		code = aio_socket_send_v_all(&http->write, http->socket, http->vec, http->count, http_onsend, http);
		if (0 != code)
		{
			http->callback(http->cbparam, http->parser, code);
		}
	}
}

static int http_connect(struct http_client_aio_transport_t *http, const char* ip, int port)
{
	int r = 0;
	socket_t tcp;
	socklen_t addrlen;
	struct sockaddr_storage ss;

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

	return aio_socket_connect(http->socket, (struct sockaddr*)&ss, addrlen, http_onconnect, http);
}

static int http_request(void *conn, const char* req, size_t nreq, const void* msg, size_t bytes, http_client_response callback, void *param)
{
	int r;
	struct http_client_aio_transport_t *http;
	http = (struct http_client_aio_transport_t *)conn;
	http_parser_clear(http->parser); // clear http parser status
	http->callback = callback;
	http->cbparam = param;
	http->retry = 0; // clear retry flag

	http->count = bytes > 0 ? 2 : 1;
	socket_setbufvec(http->vec, 0, (char*)req, nreq);
	socket_setbufvec(http->vec, 1, (void*)msg, bytes);

	if(invalid_aio_socket == http->socket)
		r = http_connect(http, http->pool->ip, http->pool->port);
	else
		r = aio_socket_send_v_all(&http->write, http->socket, http->vec, http->count, http_onsend, http);
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
