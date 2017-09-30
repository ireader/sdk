#include "http-request.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

struct http_request_t
{
	int version;
	size_t len;
	size_t capacity;
	char *ptr;
};

void* http_request_create(int version)
{
	struct http_request_t *req;

	if(version != HTTP_1_0 && version != HTTP_1_1)
		return NULL;

	// 4-bytes mm barrier
	req = (struct http_request_t *)malloc(sizeof(*req) + 1000 + sizeof(unsigned int));
	if(!req)
		return NULL;

	memset(req, 0, sizeof(*req));
	req->ptr = (char*)(req + 1);
	req->capacity = 1000;
	req->version = version;

#if defined(DEBUG) || defined(_DEBUG)
	*(unsigned int*)((char*)req->ptr + req->capacity) = 0x9FEDCBA8;
#endif
	return req;
}

void http_request_destroy(void* p)
{
	struct http_request_t *req;
	req = (struct http_request_t *)p;
#if defined(DEBUG) || defined(_DEBUG)
	assert(0x9FEDCBA8 == *(unsigned int*)((char*)req->ptr + req->capacity));
#endif

	if(req->ptr != (char*)(req + 1))
	{
		assert(1000 != req->capacity);
		free(req->ptr);
	}

#if defined(DEBUG) || defined(_DEBUG)
	memset(req, 0xCC, sizeof(*req));
#endif
	free(req);
}

const char* http_request_get(void* p, int* bytes)
{
	struct http_request_t *req;
	req = (struct http_request_t *)p;
	if(bytes) *bytes = (int)req->len;
	return req->ptr;
}

int http_request_set_uri(void* p, int method, const char* uri)
{
	struct http_request_t *req;
	static const char *s_method[] = { "GET", "POST" };
	static const char *s_version[] = { "1.0", "1.1" };

	assert(method==HTTP_GET || method==HTTP_POST);
	if(method < 0 || method >= sizeof(s_method)/sizeof(s_method[0]))
		return -1;

	req = (struct http_request_t *)p;
	assert(HTTP_1_0==req->version || HTTP_1_1==req->version);
	req->len = snprintf(req->ptr, req->capacity, 
		"%s %s HTTP/%s\r\n\r\n", 
		s_method[((unsigned int)method) % 2], 
		uri, 
		s_version[((unsigned int)req->version) % 2]);
	if(req->len+1 >= req->capacity)
		return -1; // buffer full
	return 0;
}

int http_request_set_host(void* req, const char* ip, int port)
{
	char host[256] = {0};
	if(sizeof(host) <= snprintf(host, sizeof(host), "%s:%d", ip, port) + 1)
		return -1;
	return http_request_set_header(req, "Host", host);
}

int http_request_set_cookie(void* req, const char* cookie)
{
	return http_request_set_header(req, "Cookie", cookie);
}

int http_request_set_content_lenth(void* req, unsigned int bytes)
{
	char length[32] = { 0 };
	if (sizeof(length) <= snprintf(length, sizeof(length), "%u", bytes) + 1)
		return -1;
	return http_request_set_header(req, "Content-Length", length);
}

int http_request_set_content_type(void* req, const char* value)
{
	return http_request_set_header(req, "Content-Type", value);
}

int http_request_set_header_int(void* req, const char* name, int value)
{
	char length[32] = {0};
	if (sizeof(length) <= snprintf(length, sizeof(length), "%d", value) + 1)
		return -1;
	return http_request_set_header(req, name, length);
}

int http_request_set_header(void* request, const char* name, const char* value)
{
	size_t nc, vc;
	struct http_request_t *req;
	req = (struct http_request_t *)request;

	nc = strlen(name);
	vc = strlen(value);
	if(req->len + nc + vc + 4 >= req->capacity)
	{
		char *p;
		if(req->ptr == (char*)(req + 1))
			p = (char *)malloc(req->capacity + nc + vc + 1024 + sizeof(unsigned int));
		else
			p = (char *)realloc(req->ptr, req->capacity + nc + vc + 1024 + sizeof(unsigned int));
		if(!p)
			return ENOMEM;

		// copy content
		if(req->ptr == (char*)(req + 1))
			memcpy(p, req->ptr, req->capacity);

		req->ptr = p;
		req->capacity += nc + vc + 1024;
#if defined(DEBUG) || defined(_DEBUG)
		*(unsigned int*)((char*)req->ptr + req->capacity) = 0x9FEDCBA8;
#endif
	}

	assert(req->len > 0);
	req->len += snprintf(req->ptr + (req->len-2), req->capacity - (req->len-2), "%s: %s\r\n\r\n", name, value) - 2;

#if defined(DEBUG) || defined(_DEBUG)
	assert(0x9FEDCBA8 == *(unsigned int*)((char*)req->ptr + req->capacity));
#endif
	return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
void http_request_test(void)
{
	void* req = NULL;
	char msg[2048] = {0};

	req = http_request_create(HTTP_1_1);

	http_request_set_uri(req, HTTP_GET, "/hello");
	assert(0 == strcmp("GET /hello HTTP/1.1\r\n\r\n", http_request_get(req, NULL)));
	http_request_set_host(req, "127.0.0.1", 80);
	assert(0 == strcmp("GET /hello HTTP/1.1\r\nHost: 127.0.0.1:80\r\n\r\n", http_request_get(req, NULL)));
	http_request_set_cookie(req, "name=value");
	assert(0 == strcmp("GET /hello HTTP/1.1\r\nHost: 127.0.0.1:80\r\nCookie: name=value\r\n\r\n", http_request_get(req, NULL)));
	http_request_set_header(req, "Accept", "en-us");
	assert(0 == strcmp("GET /hello HTTP/1.1\r\nHost: 127.0.0.1:80\r\nCookie: name=value\r\nAccept: en-us\r\n\r\n", http_request_get(req, NULL)));
	http_request_set_header_int(req, "Keep-Alive", 100);
	assert(0 == strcmp("GET /hello HTTP/1.1\r\nHost: 127.0.0.1:80\r\nCookie: name=value\r\nAccept: en-us\r\nKeep-Alive: 100\r\n\r\n", http_request_get(req, NULL)));
	http_request_set_content_type(req, "application/json");
	assert(0 == strcmp("GET /hello HTTP/1.1\r\nHost: 127.0.0.1:80\r\nCookie: name=value\r\nAccept: en-us\r\nKeep-Alive: 100\r\nContent-Type: application/json\r\n\r\n", http_request_get(req, NULL)));
	http_request_set_content_lenth(req, 100);
	assert(0 == strcmp("GET /hello HTTP/1.1\r\nHost: 127.0.0.1:80\r\nCookie: name=value\r\nAccept: en-us\r\nKeep-Alive: 100\r\nContent-Type: application/json\r\nContent-Length: 100\r\n\r\n", http_request_get(req, NULL)));

	// test max-length header
	memset(msg, '-', sizeof(msg)-1);
	http_request_set_header(req, "max-length", msg);
	http_request_destroy(req);
}
#endif
