#include "ring-buffer.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

int ring_buffer_alloc(struct ring_buffer_t* rb, size_t capacity)
{
	rb->ptr = (uint8_t*)malloc(capacity);
	if (NULL == rb->ptr) return -ENOMEM;
	rb->capacity = capacity;
	rb->offset = 0;
	rb->count = 0;
	return 0;
}

int ring_buffer_free(struct ring_buffer_t* rb)
{
	if (!rb || !rb->ptr)
		return -EINVAL;
	
	free(rb->ptr);
	rb->ptr = NULL;
	rb->capacity = 0;
	ring_buffer_clear(rb);
	return 0;
}

void ring_buffer_clear(struct ring_buffer_t* rb)
{
	rb->offset = 0;
	rb->count = 0;
}

int ring_buffer_write(struct ring_buffer_t* rb, const void* data, size_t bytes)
{
	size_t n, write;
	const uint8_t* p;
	p = (const uint8_t*)data;
	
	if (bytes + rb->count > rb->capacity)
		return -E2BIG;

	write = (rb->offset + rb->count) % rb->capacity;
	n = (bytes + write) < rb->capacity ? bytes : (rb->capacity - write);
	memcpy(rb->ptr + write, p, n);

	if (n < bytes)
	{
		assert(rb->offset >= bytes - n);
		memcpy(rb->ptr, p + n, bytes - n);
	}

	rb->count += bytes;
	return 0;
}

int ring_buffer_read(struct ring_buffer_t* rb, void* data, size_t bytes)
{
	size_t n;
	uint8_t* p;
	p = (uint8_t*)data;

	if (bytes > rb->count)
		return -ENOMEM;

	n = (bytes + rb->offset) < rb->capacity ? bytes : (rb->capacity - rb->offset);
	memcpy(p, rb->ptr + rb->offset, n);

	if (n < bytes)
	{
		memcpy(p + n, rb->ptr, bytes - n);
	}

	rb->count -= bytes;
	rb->offset = (rb->offset + bytes) % rb->capacity;
	return 0;
}

size_t ring_buffer_size(struct ring_buffer_t* rb)
{
	return rb->count;
}

size_t ring_buffer_space(struct ring_buffer_t* rb)
{
	assert(rb->capacity >= rb->count);
	return rb->capacity - rb->count;
}

int ring_buffer_resize(struct ring_buffer_t* rb, size_t capacity)
{
	void* ptr;
	size_t offset;
	size_t extend;

	if (capacity < rb->count)
		return -ENOSPC;
	else if (capacity == rb->capacity)
		return 0;

	extend = capacity < rb->capacity ? rb->capacity - capacity : (capacity - rb->capacity);
	if (capacity < rb->capacity && rb->offset + rb->count > capacity)
	{
		// move toward to FRONT (|<--|)
		offset = rb->offset - extend;
		memmove(rb->ptr + offset, rb->ptr + rb->offset, rb->capacity - rb->offset);
		rb->offset = offset;
	}

	ptr = realloc(rb->ptr, capacity);
	if (!ptr)
		return -ENOMEM;

	if (capacity > rb->capacity && rb->offset + rb->count > rb->capacity)
	{
		// move toward to TAIL (|-->|)
		offset = rb->offset + extend;
		memmove(rb->ptr + offset, rb->ptr + rb->offset, rb->capacity - rb->offset);
		rb->offset = offset;
	}

	rb->ptr = ptr;
	rb->capacity = capacity;
	return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
#include <math.h>
#include <time.h>

#define N 128000

void ring_buffer_test(void)
{
	int n;
	int i, j;
	struct ring_buffer_t rb;
	uint8_t *src, *dst;

	srand((unsigned int)time(NULL));

	src = malloc(N);
	dst = malloc(N);
	for (i = 0; i < N; i++)
		src[i] = (uint8_t)(rand() % 256);

	assert(0 == ring_buffer_alloc(&rb, 1000));

	assert(0 == ring_buffer_write(&rb, src, 888));
	assert(0 != ring_buffer_write(&rb, src, 1000 - 888 + 1)); // overrun(overflow)
	assert(0 == ring_buffer_write(&rb, src + 888, 1000 - 888)); // write full
	assert(0 != ring_buffer_write(&rb, src + 1000, 1)); // overrun(overflow)
	assert(0 != ring_buffer_write(&rb, src + 1000, 1000));
	
	assert(1000 == ring_buffer_size(&rb));
	assert(0 == ring_buffer_read(&rb, dst, 100));
	assert(900 == ring_buffer_size(&rb));
	assert(0 != ring_buffer_read(&rb, dst + 100, 901)); // underrun(hungry)
	assert(0 == ring_buffer_read(&rb, dst + 100, 900));
	assert(0 == ring_buffer_size(&rb));
	assert(0 == memcmp(src, dst, 1000));

	i = 1000;
	j = 1000;
	while (i < N)
	{
		n = 1000 - (int)ring_buffer_size(&rb);
		n = (N - i) > n ? n : (N - i);
		n = rand() % (n + 1);

		assert(0 == ring_buffer_write(&rb, src + i, n));
		i += n;

		n = rand() % (ring_buffer_size(&rb) + 1);
		assert(0 == ring_buffer_read(&rb, dst + j, n));
		j += n;

		assert(j <= i);
	}

	n = (int)ring_buffer_size(&rb);
	assert(0 == ring_buffer_read(&rb, dst + j, n));
	j += n;

	assert(0 == memcmp(src, dst, N));
	ring_buffer_free(&rb);
	free(src);
	free(dst);
}
#endif
