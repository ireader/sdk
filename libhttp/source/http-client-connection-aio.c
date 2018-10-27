#include "http-client-internal.h"
#include "aio-client.h"
#include <stdlib.h>
#include <errno.h>

#define http_entry(ptr, type, member) ((type*)((char*)ptr-(ptrdiff_t)(&((type*)0)->member)))

struct http_client_aio_t
{
	aio_client_t* client;
	char buffer[2 * 1024];

	int count;
	socket_bufvec_t vec[2];

	int error;
	int retry;
};

static void http_ondestroy(void* param);
static void http_onrecv(void* param, int code, size_t bytes);
static void http_onsend(void* param, int code, size_t bytes);

static void* http_aio_create(struct http_client_t *http)
{
	struct http_client_aio_t* aio;
	aio = calloc(1, sizeof(struct http_client_aio_t));
	if (aio)
	{
		struct aio_client_handler_t handler;
		memset(&handler, 0, sizeof(handler));
		handler.ondestroy = http_ondestroy;
		handler.onrecv = http_onrecv;
		handler.onsend = http_onsend;
		aio->client = aio_client_create(http->host, http->port, &handler, &http->connection);
	}
	return aio;
}

static void http_aio_destroy(struct http_client_t* http)
{
	struct http_client_aio_t* aio;
	aio = (struct http_client_aio_t*)http->connection;

	if (aio->client)
	{
		aio_client_destroy(aio->client);
		aio->client = NULL;
	}
	free(aio);
	http->connection = NULL;
}

static void http_aio_timeout(struct http_client_t* http, int conn, int recv, int send)
{
	struct http_client_aio_t* aio;
	aio = (struct http_client_aio_t*)http->connection;
	aio_client_settimeout(aio->client, conn, recv, send);
}

static void http_ondestroy(void* param)
{
	//struct http_client_aio_t* aio;
	//aio = *(struct http_client_aio_t**)param;
	(void)param;
}

static void http_onrecv(void* param, int code, size_t bytes)
{
	struct http_client_t* http;
	struct http_client_aio_t *aio;
	aio = *(struct http_client_aio_t**)param;
	http = http_entry(param, struct http_client_t, connection);
	assert(http->connection == aio);

	if (0 == code)
	{
		size_t n = bytes;
		code = http_parser_input(http->parser, aio->buffer, &n);
		if (code <= 0)
		{
			// Connection: close
			if (code < 0 || 1 == http_get_connection(http->parser))
			{
				aio_client_disconnect(aio->client);
			}
			assert(0 == n);
			http_client_handle(http, code);
			return;
		}
		else
		{
			if (0 != bytes)
			{
				// read more
				code = aio_client_recv(aio->client, aio->buffer, sizeof(aio->buffer));
			}
			else
			{
				if (1 == aio->retry)
				{
					// try resend
					aio->retry = 0;
					code = aio_client_send_v(aio->client, aio->vec, aio->count);
				}
				else
				{
					code = ECONNRESET; // peer close
				}
			}
		}
	}
	
	if(0 != code)
	{
		http_client_handle(http, code);
	}
}

static void http_onsend(void* param, int code, size_t bytes)
{
	struct http_client_t* http;
	struct http_client_aio_t *aio;
	aio = *(struct http_client_aio_t**)param;
	http = http_entry(param, struct http_client_t, connection);
	assert(http->connection == aio);

	if (0 == code)
		code = aio_client_recv(aio->client, aio->buffer, sizeof(aio->buffer));

	if (0 != code && 1 == aio->retry)
	{
		// try resend
		aio->retry = 0;
		code = aio_client_send_v(aio->client, aio->vec, aio->count);
	}

	if (0 != code)
	{
		http_client_handle(http, code);
	}

	(void)bytes;
}

static int http_aio_request(struct http_client_t* http, const char* req, size_t nreq, const void* msg, size_t bytes)
{
	struct http_client_aio_t* aio;
	aio = (struct http_client_aio_t*)http->connection;
	http_parser_clear(http->parser); // clear http parser status

	socket_setbufvec(aio->vec, 0, (char*)req, nreq);
	socket_setbufvec(aio->vec, 1, (void*)msg, bytes);
	aio->count = bytes > 0 ? 2 : 1;

	aio->retry = 1; // try again if send failed
	if (0 != aio_client_send_v(aio->client, aio->vec, aio->count))
	{
		// connection reset ???
		aio->retry = 0;
		return aio_client_send_v(aio->client, aio->vec, aio->count);
	}
	return 0;
}

struct http_client_connection_t* http_client_connection_aio(void)
{
	static struct http_client_connection_t conn = {
		http_aio_create,
		http_aio_destroy,
		http_aio_timeout,
		http_aio_request,
	};
	return &conn;
}
