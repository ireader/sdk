#include "http-bundle.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "http-server.h"
#include "cstringext.h"

void* http_bundle_alloc(size_t sz)
{
	struct http_bundle_t *bundle;

	bundle = malloc(sizeof(struct http_bundle_t) + sz + sizeof(unsigned int));
	if(!bundle)
		return NULL;

	bundle->ref = 1;
	bundle->len = 0;
	bundle->ptr = (char*)(bundle + 1);
	bundle->capacity = sz;

#if defined(_DEBUG) || defined(DEBUG)
	*(unsigned int*)((char*)bundle->ptr + bundle->capacity) = 0x9FEDCBA8;
#endif
	return bundle;
}

int http_bundle_free(void* ptr)
{
	struct http_bundle_t *bundle;
	bundle = (struct http_bundle_t *)ptr;
	http_bundle_release(bundle);
	return 0;
}

void* http_bundle_lock(void* ptr)
{
	struct http_bundle_t *bundle;
	bundle = (struct http_bundle_t *)ptr;
	return bundle->ptr;
}

int http_bundle_unlock(void* ptr, size_t sz)
{
	struct http_bundle_t *bundle;
	bundle = (struct http_bundle_t *)ptr;
#if defined(_DEBUG) || defined(DEBUG)
	assert(0x9FEDCBA8 == *(unsigned int*)((char*)bundle->ptr + bundle->capacity));
#endif
	assert(sz <= bundle->capacity);
	bundle->len = sz;
	return 0;
}

int http_bundle_addref(struct http_bundle_t *bundle)
{
    atomic_increment32(&bundle->ref);
    return 0;
}

int http_bundle_release(struct http_bundle_t *bundle)
{
#if defined(_DEBUG) || defined(DEBUG)
	assert(0x9FEDCBA8 == *(unsigned int*)((char*)bundle->ptr + bundle->capacity));
#endif

	assert(bundle->len > 0);
	if(0 == atomic_decrement32(&bundle->ref))
	{
		free(bundle);
	}

	return 0;
}
