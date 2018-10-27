#include "http-client-internal.h"
#include "sockutil.h"
#include <stdlib.h>

static void* http_socket_create(http_client_t* http)
{
	http->timeout.conn = 20000;
	http->timeout.recv = 20000;
	http->timeout.send = 20000;
	return http;
}

static void http_socket_destroy(http_client_t* http)
{
	if (socket_invalid != http->socket)
	{
		socket_close(http->socket);
		http->socket = socket_invalid;
	}
}

static int http_socket_connect(http_client_t* http)
{
	// check connection
	if(socket_invalid != http->socket && 1==socket_readable(http->socket))
	{
		socket_close(http->socket);
		http->socket = socket_invalid;
	}

	if(socket_invalid == http->socket)
	{
		socket_t socket;
		socket = socket_connect_host(http->host, http->port, http->timeout.conn);
		if(socket_invalid == socket)
			return -1;

		socket_setnonblock(socket, 0); // restore block status
		http->socket = socket;
	}

	return 0;
}

static int http_socket_send(socket_t socket, int timeout, const char* req, size_t nreq, const void* msg, size_t bytes)
{
	socket_bufvec_t vec[2];
	socket_setbufvec(vec, 0, (void*)req, nreq);
	socket_setbufvec(vec, 1, (void*)msg, bytes);
	return ((int)(nreq + bytes) == socket_send_v_all_by_time(socket, vec, bytes > 0 ? 2 : 1, 0, timeout)) ? 0 : -1;
}

static int http_socket_request(http_client_t* http, const char* req, size_t nreq, const void* msg, size_t bytes)
{
	int r = -1;
	int tryagain = 0; // retry connection
	char buffer[1024] = {0};

RETRY_REQUEST:
	// clear status
	http_parser_clear(http->parser);

	// connection
	r = http_socket_connect(http);
	if(0 != r) return r;

	// send request
	r = http_socket_send(http->socket, http->timeout.send, req, nreq, msg, bytes);
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
		r = socket_recv_by_time(http->socket, buffer, sizeof(buffer), 0, http->timeout.recv);
		if(r >= 0)
		{
			// need input length 0 for http client detect server close connection
			int state;
			size_t n = (size_t)r;
			state = http_parser_input(http->parser, buffer, &n);
			if(state <= 0)
			{
				// Connection: close
				if (0 == state && 1 == http_get_connection(http->parser))
				{
					socket_close(http->socket);
					http->socket = socket_invalid;
				}
				assert(0 == n);
				http_client_handle(http, state);
				return 0;
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

static void http_socket_timeout(struct http_client_t* http, int conn, int recv, int send)
{
	assert(conn >= 0 && recv >= 0 && send >= 0);
	http->timeout.conn = conn;
	http->timeout.recv = recv;
	http->timeout.send = send;
}

struct http_client_connection_t* http_client_connection(void)
{
	static struct http_client_connection_t conn = {
		http_socket_create,
		http_socket_destroy,
		http_socket_timeout,
		http_socket_request,
	};
	return &conn;
}
