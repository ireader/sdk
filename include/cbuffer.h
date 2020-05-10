#ifndef _cbuffer_h_
#define _cbuffer_h_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cbuffer_t
{
	uint8_t* ptr;
	size_t len;
	size_t cap;
	size_t limit;
};

static inline void cbuffer_init(struct cbuffer_t* cb)
{
	memset(cb, 0, sizeof(*cb));
	cb->limit = 0x80000000U;
}

static inline void cbuffer_free(struct cbuffer_t* cb)
{
	if (cb && cb->ptr)
	{
		assert(cb->cap > 0);
		free(cb->ptr);
		cb->ptr = NULL;
		cb->len = cb->cap = 0;
	}
}

static inline int cbuffer_resize(struct cbuffer_t* cb, size_t bytes)
{
	uint8_t* ptr;
	if (bytes <= cb->cap)
		return 0;

	if (bytes > cb->limit)
		return -1;

	bytes += bytes / 4;
	bytes = bytes > cb->limit ? cb->limit : bytes;
	ptr = (uint8_t*)realloc(cb->ptr, bytes);
	if (!ptr)
		return -1; // ENOMEM

	cb->ptr = ptr;
	cb->cap = bytes;
	return 0;
}

static inline int cbuffer_append(struct cbuffer_t* cb, const void* data, size_t bytes)
{
	if (cb->len + bytes > cb->cap && 0 != cbuffer_resize(cb, cb->len + bytes))
		return -1;
	
	memmove(cb->ptr + cb->len, data, bytes);
	cb->len += bytes;
	return (int)cb->len;
}

static inline int cbuffer_insert(struct cbuffer_t* cb, size_t off, const void* data, size_t bytes)
{
	if (off > cb->len)
		return -1;

	if (cb->len + bytes > cb->cap && 0 != cbuffer_resize(cb, cb->len + bytes))
		return -1;

	memmove(cb->ptr + off + bytes, cb->ptr + off, cb->len - off);
	memmove(cb->ptr + off, data, bytes);
	cb->len += bytes;
	return (int)cb->len;
}

static inline int cbuffer_remove(struct cbuffer_t* cb, size_t off, size_t bytes)
{
	if (off + bytes > cb->len)
		return -1;

	memmove(cb->ptr + off, cb->ptr + off + bytes, cb->len - off - bytes);
	cb->len -= bytes;
	return (int)cb->len;
}

#ifdef __cplusplus
}
#endif
#endif /* !_cbuffer_h_ */
