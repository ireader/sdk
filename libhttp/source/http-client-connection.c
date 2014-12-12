#include "http-client-connect.h"


//////////////////////////////////////////////////////////////////////////
///
static int http_setcookie(struct http_pool_t* pool, const char* cookie, size_t bytes)
{
	if(pool->ncookie < bytes+1)
	{
		char* p;
		p = (char*)malloc(bytes+1);
		if(!p)
			return ENOMEM;
		pool->cookie = p;
		pool->ncookie = bytes+1;
	}

	memcpy(pool->cookie, cookie, bytes);
	pool->cookie[bytes] = '\0';
	return 0;
}

static int http_add_header(struct http_connection_t* http, const char* name, const char* value, size_t bytes)
{
	size_t nc;

	if(0 == bytes)
		return 0; // ignore

	nc = strlen(name);

	if(nc + bytes + 4 > http->maxreq)
	{
		char *p;
		p = (char *)malloc(http->maxreq + nc + bytes + 4 + 512);
		if(!p)
			return ENOMEM;

		http->req = p;
		http->maxreq += nc + bytes + 4 + 512;
	}

	http->nreq += sprintf(http->req+http->nreq, "%s: %s\r\n", name, value);
	return 0;
}

static int http_add_header_int(struct http_connection_t* http, const char* name, int value)
{
	size_t nc;

	nc = strlen(name);
	if(nc + 16 > http->maxreq)
	{
		char *p;
		p = (char *)malloc(http->maxreq + nc + 16 + 512);
		if(!p)
			return ENOMEM;

		http->req = p;
		http->maxreq += nc + 16 + 512;
	}

	http->nreq += sprintf(http->req+http->nreq, "%s: %d\r\n", name, value);
	return 0;
}

// POST http://ip:port/xxx HTTP/1.1\r\n
// HOST: ip:port\r\n
// Content-Length: bytes\r\n
// Content-Type: application/x-www-form-urlencoded\r\n
static int http_make_request(struct http_connection_t* http, const char* method, const char* uri, const struct http_header_t *headers, size_t n, size_t bytes)
{
	size_t i;
	const char* content_type = "";

	// Request Line
	assert(http->maxreq > 0 && http->req);
	http->nreq = snprintf(http->req, http->maxreq, "%s %s HTTP/1.1\r\n", method, uri);

	// Host header
	assert(http->nreq > 0 && http->nreq < http->maxreq);
	if(80 == http->pool->port)
		http->nreq += snprintf(http->req, http->maxreq-http->nreq, "Host: %s\r\n", http->pool->ip);
	else
		http->nreq += snprintf(http->req, http->maxreq-http->nreq, "Host: %s:%d\r\n", http->pool->ip, http->pool->port);
	if(http->nreq == http->maxreq)
		return -1; // uri too long

	// User-defined headers
	for(i = 0; i < n; i++)
	{
		size_t nc, vc;

		nc = strlen(headers[i].name);
		vc = strlen(headers[i].value);

		assert(headers[i].name && headers[i].value);
		if(0 == stricmp("Content-Length", headers[i].name)
			|| 0 == stricmp("HOST", headers[i].name))
		{
			continue; // ignore
		}
		else if(0 == stricmp("Cookie", headers[i].name))
		{
			// update cookie
			http_setcookie(http->pool, headers[i].value, vc);
			continue;
		}
		else if(0 == strcasecmp("Content-Type", headers[i].name))
		{
			content_type = headers[i].value;
			continue;
		}

		if(nc < 1 || strchr(" :\t\r\n", headers[i].name[nc-1])
			|| vc < 1 || strchr(" \t\r\n", headers[i].value[vc-1]))
			return -1; // invalid value

		if(0 != http_add_header(http, headers[i].name, headers[i].value, vc))
			return -1;
	}

	// Add Other Header
	if( 0 == http_add_header(http, "Cookie", http->pool->cookie, http->pool->ncookie)
		&& 0 == http_add_header(http, "Content-Type", content_type, strlen(content_type))
		&& 0 == http_add_header_int(http, "Content-Length", bytes))
		return 0;
	return -1;
}

static void* http_create(struct http_pool_t *pool)
{
}

static void http_destroy(void *http)
{
}

static int http_request(void *http, const char* method, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_response callback, void *param)
{
	struct http_connection_t *http;
	http = (struct http_connection_t *)param;

	if(0 != http_make_request(http, method, uri, headers, n, bytes))
	{
		return -1;
	}

	// check writeable
	if(socket_invalid != http->socket && 0==socket_writeable(http->socket))
	{
		socket_shutdown(http->socket);
		socket_close(http->socket);
		http->socket = socket_invalid;
	}

	if(socket_invalid == http->socket)
	{
		int r;
		socket_t socket;
		socket = socket_tcp();
		r = socket_connect_ipv4_by_time(socket, http->pool->ip, http->pool->port, http->pool->wtimeout);
		if(0 != r)
		{
			socket_close(socket);
			return r;
		}

		http->socket = socket;
	}

	if(http->nreq != socket_send_all_by_time(http->socket, http->req, http->nreq, http->pool->wtimeout))
	{
		socket_shutdown(http->socket);
		socket_close(http->socket);
		http->socket = socket_invalid;
		return -1; // send failed(timeout)
	}

	if(bytes > 0 && bytes != socket_send_all_by_time(http->socket, msg, bytes, http->pool->wtimeout))
	{
		socket_shutdown(http->socket);
		socket_close(http->socket);
		http->socket = socket_invalid;
		return -1; // send failed(timeout)
	}

	while(socket_invalid != http->socket)
	{
		r = socket_recv_by_time(http->socket, http->req, http->maxreq, 0, http->pool->rtimeout);
		if(r <= 0)
		{
			socket_close(http->socket);
			http->socket = socket_invalid;
			return 0==r ? -1 : r;
		}

		if(0 == http_parser_input(http->parser, http->req, &r))
		{
			assert(0 == r);
			// parse cookie

			callback(param, http, 0);
			break;
		}
	}

	return 0;
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
