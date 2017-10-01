#include "http-client-internal.h"
#include "sockutil.h"
#include "aio-connect.h"
#include "aio-rwutil.h"
#include "aio-recv.h"
#include <stdlib.h>

struct http_client_aio_t
{
	aio_socket_t socket;
	struct aio_recv_t recv;
	struct aio_socket_rw_t send;
	char buffer[2 * 1024];

	int count;
	socket_bufvec_t vec[2];

	int error;
	int retry;
};

static void http_onrecv(void* param, int code, size_t bytes);
static void http_onsend(void* param, int code, size_t bytes);
static void http_onconnect(void* param, aio_socket_t socket, int code);

static void* http_aio_create(struct http_client_t *http)
{
	(void)http;
	return calloc(1, sizeof(struct http_client_aio_t));
}

static void http_aio_destroy(void *param)
{
	struct http_client_t* http;
	struct http_client_aio_t* aio;
	http = (struct http_client_t*)param;
	aio = (struct http_client_aio_t*)http->connection;

	if (aio->socket)
	{
		aio_socket_destroy(aio->socket, NULL, NULL);
		aio->socket = invalid_aio_socket;
	}
	free(aio);
	http->connection = NULL;
}

static void http_onrecv(void* param, int code, size_t bytes)
{
	struct http_client_t* http;
	struct http_client_aio_t *aio;
	http = (struct http_client_t*)param;
	aio = (struct http_client_aio_t *)http->connection;

	if (0 == code)
	{
		size_t n = bytes;
		code = http_parser_input(http->parser, aio->buffer, &n);
		if (code <= 0)
		{
			// Connection: close
			if (code < 0 || 1 == http_get_connection(http->parser))
			{
				assert(invalid_aio_socket != aio->socket);
				aio_socket_destroy(aio->socket, NULL, NULL);
				aio->socket = invalid_aio_socket;
			}
			assert(0 == n);
			http_client_handle(http, code);
			return;
		}
		else
		{
			// read more
			code = aio_recv(&aio->recv, http->timeout.recv, aio->socket, aio->buffer, sizeof(aio->buffer), http_onrecv, http);
		}
	}
	
	if(0 != code)
	{
		assert(invalid_aio_socket != aio->socket);
		aio_socket_destroy(aio->socket, NULL, NULL);
		aio->socket = invalid_aio_socket;
		http_client_handle(http, code);
	}
}

static void http_onsend(void* param, int code, size_t bytes)
{
	struct http_client_t* http;
	struct http_client_aio_t* aio;
	http = (struct http_client_t*)param;
	aio = (struct http_client_aio_t*)http->connection;

	if (0 == code)
		code = aio_recv(&aio->recv, http->timeout.recv, aio->socket, aio->buffer, sizeof(aio->buffer), http_onrecv, http);

	if (0 != code)
	{
		// close socket and try again
		assert(invalid_aio_socket != aio->socket);
		aio_socket_destroy(aio->socket, NULL, NULL);
		aio->socket = invalid_aio_socket;

		if(1 == aio->retry)
			code = aio_connect(http->host, http->port, http->timeout.conn, http_onconnect, http);
	}

	if (0 != code)
	{
		http_client_handle(http, code);
	}

	(void)bytes;
}

static void http_onconnect(void* param, aio_socket_t socket, int code)
{
	struct http_client_t* http;
	struct http_client_aio_t* aio;
	http = (struct http_client_t*)param;
	aio = (struct http_client_aio_t*)http->connection;

	if (0 != code)
	{
		assert(invalid_aio_socket == aio->socket);
		http_client_handle(http, code);
	}
	else
	{
		aio->retry = 0;
		aio->socket = socket;
		code = aio_socket_send_v_all(&aio->send, http->timeout.send, aio->socket, aio->vec, aio->count, http_onsend, http);
		if (0 != code)
		{
			aio_socket_destroy(aio->socket, NULL, NULL);
			aio->socket = invalid_aio_socket;
			http_client_handle(http, code);
		}
	}
}

static int http_aio_request(struct http_client_t* http, const char* req, size_t nreq, const void* msg, size_t bytes)
{
	int r = 0;
	struct http_client_aio_t* aio;
	aio = (struct http_client_aio_t*)http->connection;
	http_parser_clear(http->parser); // clear http parser status

	socket_setbufvec(aio->vec, 0, (char*)req, nreq);
	socket_setbufvec(aio->vec, 1, (void*)msg, bytes);
	aio->count = bytes > 0 ? 2 : 1;

	if (invalid_aio_socket == aio->socket)
	{
		r = aio_connect(http->host, http->port, http->timeout.conn, http_onconnect, http);
	}
	else
	{
		aio->retry = 1; // try again if send failed
		r = aio_socket_send_v_all(&aio->send, http->timeout.send, aio->socket, aio->vec, aio->count, http_onsend, http);
	}
	return r;
}

struct http_client_connection_t* http_client_connection_aio()
{
	static struct http_client_connection_t conn = {
		http_aio_create,
		http_aio_destroy,
		http_aio_request,
	};
	return &conn;
}
