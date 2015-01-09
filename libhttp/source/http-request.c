#include "http-request.h"
#include "cstringext.h"

struct http_request_t
{
	int version;
	char *ptr;
	size_t len;
	size_t capacity;
};

void* http_request_create(int version)
{
	struct http_request_t *req;

	if(version != HTTP_1_0 && version != HTTP_1_1)
		return NULL;

	req = (struct http_request_t *)malloc(sizeof(*req) + 1000);
	if(!req)
		return NULL;

	memset(req, 0, sizeof(*req));
	req->ptr = (char*)(req + 1);
	req->capacity = 1000;
	req->version = version;

	return req;
}

void http_request_destroy(void* p)
{
	struct http_request_t *req;
	req = (struct http_request_t *)p;

	if(req->ptr != (char*)(req + 1))
	{
		assert(1000 != req->capacity);
		free(req->ptr);
	}

	free(req);
}

const char* http_request_get(void* p)
{
	struct http_request_t *req;
	req = (struct http_request_t *)p;
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
	req->len = snprintf(req->ptr, req->capacity, "%s %s HTTP/%s\r\n\r\n", s_method[method], uri, s_version[req->version]);
	if(req->len+1 >= req->capacity)
		return -1; // buffer full
	return 0;
}

int http_request_set_host(void* req, const char* ip, int port)
{
	char host[64] = {0};
	if(snprintf(host, sizeof(host), "%s:%d", ip, port)+1 >= sizeof(host))
		return -1;
	return http_request_set_header(req, "Host", host);
}

int http_request_set_cookie(void* req, const char* cookie)
{
	return http_request_set_header(req, "Cookie", cookie);
}

int http_request_set_content_lenth(void* req, unsigned int bytes)
{
	char length[16] = {0};
	snprintf(length, sizeof(length), "%u", bytes);
	return http_request_set_header(req, "Content-Length", length);
}

int http_request_set_content_type(void* req, const char* value)
{
	return http_request_set_header(req, "Content-Type", value);
}

int http_request_set_header_int(void* req, const char* name, int value)
{
	char length[16] = {0};
	snprintf(length, sizeof(length), "%d", value);
	return http_request_set_header(req, name, length);
}

int http_request_set_header(void* request, const char* name, const char* value)
{
	size_t nc, vc;
	struct http_request_t *req;
	req = (struct http_request_t *)request;

	nc = strlen(name);
	vc = strlen(value);
	if(nc + vc + 4 >= req->capacity)
	{
		char *p;
		if(req->ptr == (char*)(req + 1))
			p = (char *)malloc(req->capacity + nc + vc + 1024);
		else
			p = (char *)realloc(req->ptr, req->capacity + nc + vc + 1024);
		if(!p)
			return ENOMEM;

		req->ptr = p;
		req->capacity += nc + vc + 1024;
	}

	assert(req->len > 0);
	req->len += snprintf(req->ptr + (req->len-2), req->capacity - (req->len-2), "%s: %s\r\n\r\n", name, value) - 2;
	return 0;
}
