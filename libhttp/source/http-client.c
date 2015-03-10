#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
#include "cstringext.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "http-parser.h"
#include "http-cookie.h"
#include "http-client.h"
#include "http-request.h"
#include "http-client-connect.h"

//////////////////////////////////////////////////////////////////////////
/// HTTP Pool API
void http_pool_release(struct http_pool_t *pool)
{
	size_t i;
	assert(pool->ref > 0);
	if(0 == atomic_decrement32(&pool->ref))
	{
		for(i = 0; i < MAX_HTTP_CONNECTION; i++)
		{
			assert(0 == pool->https[i].clock);
			if(pool->https[i].request)
				http_request_destroy(pool->https[i].request);
			if(pool->https[i].http)
				pool->api->destroy(pool->https[i].http);
		}

		locker_destroy(&pool->locker);

#if defined(_DEBUG) || defined(DEBUG)
		memset(pool, 0xCC, sizeof(*pool));
#endif
		free(pool);
	}
}

static struct http_conn_t* http_pool_fetch(struct http_pool_t *pool)
{
	size_t i;
	struct http_conn_t* http = NULL;

	locker_lock(&pool->locker);
	for(i = 0; i < MAX_HTTP_CONNECTION; i++)
	{
		if(0 == pool->https[i].clock)
		{
			http = &pool->https[i];
			http->pool = pool;
			http->clock = time64_now();
			assert(++http->count > 0);
			break;
		}
	}

	if(http)
	{
		if(!http->request)
			http->request = http_request_create(HTTP_1_1);
		
		if(!http->http)
			http->http = pool->api->create(pool);

		if(!http->http || !http->request)
			http->clock = 0;
	}
	locker_unlock(&pool->locker);

	return (http && http->http && http->request) ? http : NULL;
}

static int http_pool_setcookie(struct http_pool_t* pool, const char* cookie, size_t bytes)
{
	if(pool->ncookie < bytes+1)
	{
		char* p;
		p = (char*)realloc(pool->cookie, bytes+1);
		if(!p)
			return ENOMEM;
		pool->cookie = p;
		pool->ncookie = bytes+1;
	}

	memcpy(pool->cookie, cookie, bytes);
	pool->cookie[bytes] = '\0';
	return 0;
}

// POST http://ip:port/xxx HTTP/1.1\r\n
// HOST: ip:port\r\n
// Content-Length: bytes\r\n
// Content-Type: application/x-www-form-urlencoded\r\n
static int http_make_request(struct http_pool_t *pool, void *req, int method, const char* uri, const struct http_header_t *headers, size_t n, size_t bytes)
{
	size_t i;
	const char* content_type = "text/html";

	// Request Line
	http_request_set_uri(req, method, uri);
	http_request_set_host(req, pool->ip, pool->port);

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
			http_pool_setcookie(pool, headers[i].value, vc);
			continue;
		}
		else if(0 == stricmp("Content-Type", headers[i].name))
		{
			content_type = headers[i].value;
			continue;
		}

		if(nc < 1 || strchr(" :\t\r\n", headers[i].name[nc-1])
			|| vc < 1 || strchr(" \t\r\n", headers[i].value[vc-1]))
			return -1; // invalid value

		if(0 != http_request_set_header(req, headers[i].name, headers[i].value))
			return -1;
	}

	// Add Other Header
	if( (!pool->cookie || 0 == http_request_set_header(req, "Cookie", pool->cookie))
		&& 0 == http_request_set_header(req, "Content-Type", content_type)
		&& 0 == http_request_set_header_int(req, "Content-Length", bytes))
		return 0;
	return -1;
}

static void http_client_onaction(void *param, void *parser, int code)
{
	char buffer[512];
	const char* cookie;
	struct http_conn_t *http;
	http = (struct http_conn_t *)param;

	if(0 == code)
	{
		// handle cookie
		cookie = http_get_cookie(parser);
		if(cookie)
		{
			cookie_t ck;
			ck = http_cookie_parse(cookie, strlen(cookie));
			if(ck)
			{
				const char *cookiename, *cookievalue;
				cookiename = http_cookie_get_name(ck);
				cookievalue = http_cookie_get_value(ck);
				// TODO: check cookie buffer length
				assert(cookiename && cookievalue);
				assert(STRLEN(cookiename) + STRLEN(cookievalue) + 1 < sizeof(buffer));
				snprintf(buffer, sizeof(buffer), "%s=%s", cookiename, cookievalue);
				http_pool_setcookie(http->pool, buffer, strlen(buffer));
			}
		}
	}

	http->callback(http->cbparam, parser, code);
	assert(0 != http->clock);
	http->clock = 0;
}

static int http_client_request(struct http_pool_t *pool, int method, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_response callback, void *param)
{
	int r;
	size_t nreq;
	const char* req;
	struct http_conn_t *http;

	http = http_pool_fetch(pool);
	if(!http)
		return -1;

	http->callback = callback;
	http->cbparam = param;
	if(0 != http_make_request(pool, http->request, method, uri, headers, n, bytes))
	{
		return -1;
	}

	req = http_request_get(http->request);
	nreq = strlen(req);
	r = pool->api->request(http->http, req, nreq, msg, bytes, http_client_onaction, http);
	if(0 != r)
	{
		// push back
		http->clock = 0;
	}

	return r;
}

void* http_client_create(const char* ip, unsigned short port, int flags)
{
	struct http_pool_t* pool;

	if(!ip || strlen(ip) >= sizeof(pool->ip))
		return NULL;

	pool = (struct http_pool_t *)malloc(sizeof(*pool));
	if(!pool) return NULL;

	memset(pool, 0, sizeof(*pool));
	locker_create(&pool->locker);
	pool->ref = 1;
	strcpy(pool->ip, ip);
	pool->port = port;
	pool->wtimeout = 10000;
	pool->rtimeout = 10000;

	pool->api = 0 == flags ? http_client_connection_aio() : http_client_connection_poll();
	return pool;
}

void http_client_destroy(void *client)
{
	int32_t i;
	struct http_pool_t *pool;
	pool = (struct http_pool_t *)client;

	// synchronize stop running(wait for callback)
	locker_lock(&pool->locker);
	for(i = 0; i < MAX_HTTP_CONNECTION; i++)
	{
		if(!pool->https[i].http)
			break;

		pool->api->destroy(pool->https[i].http);
		pool->https[i].http = NULL;
	}
	locker_unlock(&pool->locker);

	http_pool_release(pool);
}

void http_client_recycle(void* client)
{
	int32_t i;
	void *http;
	time64_t clock;
	struct http_pool_t *pool;
	pool = (struct http_pool_t *)client;
	clock = time64_now();

RECYCLE_AGAIN:
	http = NULL;
	locker_lock(&pool->locker);
	for(i = 0; i < MAX_HTTP_CONNECTION; i++)
	{
		if(0 == pool->https[i].clock)
			continue;

		assert(pool->https[i].http);
		if(abs((int)(clock - pool->https[i].clock)) > 2*60*1000)
		{
			http = pool->https[i].http;
			pool->https[i].http = NULL;
			pool->https[i].clock = 0;
			break;
		}
	}
	locker_unlock(&pool->locker);

	if(http)
	{
		pool->api->destroy(http);
		goto RECYCLE_AGAIN;
	}
}

int http_client_get(void* client, const char* uri, const struct http_header_t *headers, size_t n, http_client_response callback, void *param)
{
	return http_client_request(client, HTTP_GET, uri, headers, n, NULL, 0, callback, param);
}

int http_client_post(void* client, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_response callback, void *param)
{
	return http_client_request(client, HTTP_POST, uri, headers, n, msg, bytes, callback, param);
}

const char* http_client_get_header(void* parser, const char *name)
{
	return http_get_header_by_name(parser, name);
}

int http_client_get_content(void* parser, const void **content, size_t *bytes)
{
	*bytes = http_get_content_length(parser);
	*content = http_get_content(parser);
	return 0;
}

int http_client_init(void)
{
	return 0;
}

int http_client_cleanup(void)
{
	return 0;
}
