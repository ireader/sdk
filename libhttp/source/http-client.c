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
#include "http-client-connect.h"

#define MAX_HTTP_CONNECTION 5

#if defined(OS_WINDOWS)
#define strcasecmp _stricmp
#endif

//////////////////////////////////////////////////////////////////////////
/// HTTP Pool API
static void http_pool_release(struct http_pool_t *pool)
{
	assert(pool->ref > 0);
	if(0 == atomic_decrement32(&pool->ref))
	{
#if defined(_DEBUG) || defined(DEBUG)
		size_t i;
		for(i = 0; i < MAX_HTTP_CONNECTION; i++)
		{
			assert(0 == pool->https[i].running);
		}
#endif

		locker_destroy(&pool->locker);

#if defined(_DEBUG) || defined(DEBUG)
		memset(pool, 0xCC, sizeof(*pool));
#endif
		free(pool);
	}
}

static struct http_connection_t* http_pool_fetch(struct http_pool_t *pool)
{
	size_t i;
	struct http_connection_t* http = NULL;

	for(i = 0; i < MAX_HTTP_CONNECTION; i++)
	{
		if(atomic_cas32(&pool->https[i].running, 0, 1))
		{
			http = &pool->https[i];
			break;
		}
	}

	if(http)
	{
		if(0 != http_connection_init(http))
		{
			assert(1 == http->running);
			atomic_cas32(&http->running, 1, 0);
			return NULL;
		}

		assert(1 == http->ref);
		atomic_increment32(&http->ref);
	}
	return http;
}

static int http_client_request(struct http_pool_t *pool, const char* method, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_response callback, void *param)
{
	int r;
	struct http_connection_t *http;

	http = http_pool_fetch(pool);
	if(!http)
		return -1;

	return pool->api->request(http, method, uri, headers, n, msg, bytes, callback, param);
	//r = http_make_request(http, method, uri, headers, n, bytes);
	//if(0 == r)
	//{
	//	http->callback = callback;
	//	http->cbparam = param;
	//	r = http_connection_send(http, msg, bytes);
	//}

	//http_connection_release(http);
	//return r;
}

void* http_client_create(const char* ip, unsigned short port, int flags)
{
	size_t i;
	struct http_pool_t* pool;

	if(!ip || strlen(ip)+1 > sizeof(pool->ip))
		return NULL;

	pool = (struct http_pool_t *)malloc(sizeof(*pool));
	if(!pool) return NULL;

	memset(pool, 0, sizeof(*pool));
	locker_create(&pool->locker);
	pool->ref = 1 + MAX_HTTP_CONNECTION;
	strcpy(pool->ip, ip);
	pool->port = port;

	pool->api = 0 == flags ? http_client_connection_aio() : http_client_connection_poll();

	// init 1-http connection
	pool->https[0] = pool->api->create(pool);
	if(!pool->https[0])
	{
		http_client_destroy(pool);
		return NULL;
	}

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
		if(!pool->https[i])
			break;

		pool->api->destroy(&pool->https[i]);
	}
	locker_unlock(&pool->locker);

	http_pool_release(pool);
}

int http_client_get(void* client, const char* uri, const struct http_header_t *headers, size_t n, http_client_response callback, void *param)
{
	return http_client_request(client, "GET", uri, headers, n, NULL, 0, callback, param);
}

int http_client_post(void* client, const char* uri, const struct http_header_t *headers, size_t n, const void* msg, size_t bytes, http_client_response callback, void *param)
{
	return http_client_request(client, "POST", uri, headers, n, msg, bytes, callback, param);
}

const char* http_client_get_header(void* conn, const char *name)
{
	struct http_connection_t *http;
	http = (struct http_connection_t *)conn;
	return http_get_header_by_name(http->parser, name);
}

int http_client_get_content(void* conn, const void **content, size_t *bytes)
{
	struct http_connection_t *http;
	http = (struct http_connection_t *)conn;
	*bytes = http_get_content_length(http->parser);
	*content = http_get_content(http->parser);
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
