#include "http-request.h"

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
	req = (struct http_request_t *)malloc(sizeof(*req) + 1000);
}

void http_request_destroy(void* req)
{
}
