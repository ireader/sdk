#include "http-client-internal.h"
#include "http-cookie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(OS_WINDOWS)
#define strcasecmp	_stricmp
#endif

static int http_client_setcookie(struct http_client_t* http, const char* cookie, size_t bytes)
{
	if(http->ncookie < bytes+1)
	{
		char* p;
		p = (char*)realloc(http->cookie, bytes+1);
		if(!p)
			return ENOMEM;
		http->cookie = p;
		http->ncookie = bytes+1;
	}

	memcpy(http->cookie, cookie, bytes);
	http->cookie[bytes] = '\0';
	return 0;
}

// POST http://ip:port/xxx HTTP/1.1\r\n
// HOST: ip:port\r\n
// Content-Length: bytes\r\n
// Content-Type: application/x-www-form-urlencoded\r\n
static int http_make_request(struct http_client_t *http, int method, const char* uri, const struct http_header_t *headers, size_t n, size_t bytes)
{
	int r;
	size_t i, nc, vc;
	int flag_host = 0;
	int flag_cookie = 0;
	int flag_content_length = 0;

	// Request Line
	r = http_request_set_uri(http->req, method, uri);

	// User-defined headers
	for (i = 0; i < n; i++)
	{
		nc = headers[i].name ? strlen(headers[i].name) : 0;
		vc = headers[i].value ? strlen(headers[i].value) : 0;
		if (nc < 1) continue; // invalid header name

		if (0 == strcasecmp("Content-Length", headers[i].name))
		{
			flag_content_length = 1;
		}
		else if(0 == strcasecmp("HOST", headers[i].name))
		{
			flag_host = 1;
		}
		else if(0 == strcasecmp("Cookie", headers[i].name))
		{
			flag_cookie = 1;
		}

		r = http_request_set_header(http->req, headers[i].name, headers[i].value);
		if (0 != r)
			break;
	}

	// Add Other Header
	if (0 == r && 0 == flag_host)
		r = http_request_set_host(http->req, http->host, http->port);
	if (0 == r && 0 == flag_cookie && http->cookie)
		r = http_request_set_header(http->req, "Cookie", http->cookie);
	if (0 == r && 0 == flag_content_length)
		r = http_request_set_header_int(http->req, "Content-Length", bytes);
	return r;
}

void http_client_handle(struct http_client_t *http, int code)
{
	int len;
	char buffer[512];
	const char* cookie;

	if(0 == code)
	{
		// FIXME: 
		// 1. only handle one cookie item
		// 2. check cookie path
		// 3. check cookie expire
		// 4. check cookie secure

		// handle cookie
		cookie = http_get_cookie(http->parser);
		if(cookie)
		{
			http_cookie_t* ck;
			ck = http_cookie_parse(cookie, strlen(cookie));
			if(ck)
			{
				const char *cookiename, *cookievalue;
				cookiename = http_cookie_get_name(ck);
				cookievalue = http_cookie_get_value(ck);
				if (cookiename && cookievalue)
				{
					len = snprintf(buffer, sizeof(buffer), "%s=%s", cookiename, cookievalue);
					http_client_setcookie(http, buffer, len);
					assert(len + 1 < sizeof(buffer));
				}
				http_cookie_destroy(ck);
			}
		}
	}

	locker_lock(&http->locker);
	if(http->onreply)
		http->onreply(http->cbparam, code);
	locker_unlock(&http->locker);

	http_client_release(http);
}

static int http_client_request(struct http_client_t *http, int method, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_onreply onreply, void* param)
{
	int r, len;
	const char* header;

	if(0 != http_make_request(http, method, uri, headers, n, bytes))
	{
		return -1;
	}
	header = http_request_get(http->req, &len);
	
	http->cbparam = param;
	http->onreply = onreply;

	atomic_increment32(&http->ref);
	r = http->conn->request(http, header, len, msg, bytes);
	if (0 != r)
		http_client_release(http);
	return r;
}

struct http_client_t* http_client_create(const char* ip, unsigned short port, int flags)
{
	int r;
	struct http_client_t* http;

	if(!ip || 0 == *ip) return NULL;
	http = (struct http_client_t *)calloc(1, sizeof(*http));
	if(!http) return NULL;

	r = snprintf(http->host, sizeof(http->host) - 1, "%s", ip);
	http->port = port;
	http->socket = socket_invalid;
	http->conn = flags ? http_client_connection() : http_client_connection_aio();
	locker_create(&http->locker);
	http->ref = 1;
	http->req = http_request_create(HTTP_1_1);
	http->parser = http_parser_create(HTTP_PARSER_CLIENT);
	http->connection = http->conn->create(http);
	if (r <= 0 || r >= sizeof(http->host) || !http->parser || !http->req || !http->connection)
	{
		http_client_release(http);
		return NULL;
	}
	return http;
}

void http_client_destroy(struct http_client_t* http)
{
	locker_lock(&http->locker);
	http->onreply = NULL; // disable future callback
	locker_unlock(&http->locker);

	http_client_release(http);
}

void http_client_release(struct http_client_t* http)
{
	if (0 == atomic_decrement32(&http->ref))
	{
		if (http->connection)
		{
			http->conn->destroy(http);
			http->connection = NULL;
		}

		if (http->cookie)
		{
			assert(http->ncookie > 0);
			free(http->cookie);
			http->cookie = NULL;
		}

		if (http->req)
		{
			http_request_destroy(http->req);
			http->req = NULL;
		}

		if (http->parser)
		{
			http_parser_destroy(http->parser);
			http->parser = NULL;
		}

		locker_destroy(&http->locker);
		free(http);
	}
}

void http_client_set_timeout(struct http_client_t* http, int conn, int recv, int send)
{
	assert(conn >= 0 && recv >= 0 && send >= 0);
	http->conn->timeout(http, conn, recv, send);
}

int http_client_get(struct http_client_t* http, const char* uri, const struct http_header_t *headers, size_t n, http_client_onreply onreply, void* param)
{
	return http_client_request(http, HTTP_GET, uri, headers, n, NULL, 0, onreply, param);
}

int http_client_post(struct http_client_t* http, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_onreply onreply, void* param)
{
	return http_client_request(http, HTTP_POST, uri, headers, n, msg, bytes, onreply, param);
}

const char* http_client_get_header(struct http_client_t* http, const char *name)
{
	return http_get_header_by_name(http->parser, name);
}

int http_client_get_content(struct http_client_t* http, const void **content, size_t *bytes)
{
	*bytes = http_get_content_length(http->parser);
	*content = http_get_content(http->parser);
	return 0;
}
