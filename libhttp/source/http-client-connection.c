#include "cstringext.h"
#include "sys/sock.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "http-parser.h"
#include "http-client-connect.h"

struct http_client_transport_t
{
	struct http_pool_t *pool;
	socket_t socket;
	void *parser;
};

static void* http_create(struct http_pool_t *pool)
{
	struct http_client_transport_t *http;
	http = (struct http_client_transport_t *)malloc(sizeof(*http));
	if(!http)
		return NULL;

	memset(http, 0, sizeof(*http));
	http->pool = pool;
	http->socket = socket_invalid;
	http->parser = http_parser_create(HTTP_PARSER_CLIENT);
	if(!http->parser)
	{
		free(http);
		return NULL;
	}
	return http;
}

static void http_destroy(void *p)
{
	struct http_client_transport_t *http;
	http = (struct http_client_transport_t *)p;

	if(http->parser)
	{
		http_parser_destroy(http->parser);
	}

#if defined(_DEBUG) || defined(DEBUG)
	memset(http, 0xCC, sizeof(*http));
#endif
	free(http);
}

static int http_connect(struct http_client_transport_t *http)
{
	// check connection
	if(socket_invalid != http->socket && 1==socket_readable(http->socket))
	{
		socket_close(http->socket);
		http->socket = socket_invalid;
	}

	if(socket_invalid == http->socket)
	{
		int r;
		socket_t socket;
		socket = socket_tcp();
		r = socket_connect_ipv4_by_time(socket, http->pool->ip, http->pool->port, http->pool->wtimeout);
		socket_setnonblock(socket, 0); // restore block status
		if(0 != r)
		{
			socket_close(socket);
			return r;
		}

		http->socket = socket;
	}

	return 0;
}

static int http_send_request(socket_t socket, int timeout, const char* req, size_t nreq, const void* msg, size_t bytes)
{
	// TODO: use socket_send_v

	if((int)nreq != socket_send_all_by_time(socket, req, nreq, 0, timeout))
		return -1; // send failed(timeout)

	if(bytes > 0 && (int)bytes != socket_send_all_by_time(socket, msg, bytes, 0, timeout))
		return -1; // send failed(timeout)

	return 0;
}

static int http_request(void *conn, const char* req, size_t nreq, const void* msg, size_t bytes, http_client_response callback, void *param)
{
	int r = -1;
	int tryagain = 0; // retry connection
	char buffer[1024] = {0};
	struct http_client_transport_t *http;
	http = (struct http_client_transport_t *)conn;

RETRY_REQUEST:
	// clear status
	http_parser_clear(http->parser);

	// connection
	r = http_connect(http);
	if(0 != r) return r;

	// send request
	r = http_send_request(http->socket, http->pool->wtimeout, req, nreq, msg, bytes);
	if(0 != r)
	{
		socket_close(http->socket);
		http->socket = socket_invalid;
		return r; // send failed(timeout)
	}

	// recv reply
	r = 1;
	while(r > 0)
	{
		++tryagain;
		r = socket_recv_by_time(http->socket, buffer, sizeof(buffer), 0, http->pool->rtimeout);
		if(r >= 0)
		{
			// need input length 0 for http client detect server close connection
			int state;
			size_t n = (size_t)r;
			state = http_parser_input(http->parser, buffer, &n);
			if(state <= 0)
			{
				assert(0 == n);
				callback(param, http->parser, state);
				return state;
			}
		}
		else
		{
			// EPIPE/ENOTCONN
			socket_close(http->socket);
			http->socket = socket_invalid;
			if(1 == tryagain)
				goto RETRY_REQUEST;
		}
	}

	return r;
}

struct http_client_connection_t* http_client_connection_poll()
{
	static struct http_client_connection_t conn = {
		http_create,
		http_destroy,
		http_request,
	};
	return &conn;
}
