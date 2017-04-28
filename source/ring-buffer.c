#include "ring-buffer.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

struct ring_buffer_t
{
	size_t capacity;
	size_t read; // read position
	size_t size;

	uint8_t* m_buffer;
};

void* ring_buffer_create(size_t bytes)
{
	struct ring_buffer_t* rb;
	rb = (struct ring_buffer_t*)malloc(sizeof(*rb) + bytes);
	if (NULL == rb) return NULL;
	
	rb->m_buffer = (uint8_t*)(rb + 1);
	rb->capacity = bytes;
	rb->read = 0;
	rb->size = 0;

	return rb;
}

void ring_buffer_destroy(void* p)
{
	struct ring_buffer_t* rb;
	rb = (struct ring_buffer_t*)p;
	free(rb);
}

int ring_buffer_write(void* _rb, const void* data, size_t bytes)
{
	size_t n, write;
	const uint8_t* p;
	struct ring_buffer_t* rb;

	p = (const uint8_t*)data;
	rb = (struct ring_buffer_t*)_rb;
	
	if (bytes + rb->size > rb->capacity)
		return E2BIG;

	write = (rb->read + rb->size) % rb->capacity;
	n = (bytes + write) < rb->capacity ? bytes : (rb->capacity - write);
	memcpy(rb->m_buffer + write, p, n);

	if (n < bytes)
	{
		assert(rb->read >= bytes - n);
		memcpy(rb->m_buffer, p + n, bytes - n);
	}

	rb->size += bytes;
	return 0;
}

int ring_buffer_read(void* _rb, void* data, size_t bytes)
{
	size_t n;
	uint8_t* p;
	struct ring_buffer_t* rb;

	p = (uint8_t*)data;
	rb = (struct ring_buffer_t*)_rb;

	if (bytes > rb->size)
		return ENOMEM;

	n = (bytes + rb->read) < rb->capacity ? bytes : (rb->capacity - rb->read);
	memcpy(p, rb->m_buffer + rb->read, n);

	if (n < bytes)
	{
		memcpy(p + n, rb->m_buffer, bytes - n);
	}

	rb->size -= bytes;
	rb->read = (rb->read + bytes) % rb->capacity;
	return 0;
}

size_t ring_buffer_size(void* _rb)
{
	struct ring_buffer_t* rb;
	rb = (struct ring_buffer_t*)_rb;
	return rb->size;
}


#if defined(DEBUG) || defined(_DEBUG)
#include <math.h>
#include <time.h>

#define N 128000

void ring_buffer_test(void)
{
	int n;
	int i, j;
	void* rb;
	uint8_t *src, *dst;

	srand((unsigned int)time(NULL));

	src = malloc(N);
	dst = malloc(N);
	for (i = 0; i < N; i++)
		src[i] = (uint8_t)(rand() % 256);

	rb = ring_buffer_create(1000);

	assert(0 == ring_buffer_write(rb, src, 888));
	assert(0 != ring_buffer_write(rb, src, 1000 - 888 + 1)); // overrun(overflow)
	assert(0 == ring_buffer_write(rb, src + 888, 1000 - 888)); // write full
	assert(0 != ring_buffer_write(rb, src + 1000, 1)); // overrun(overflow)
	assert(0 != ring_buffer_write(rb, src + 1000, 1000));
	
	assert(1000 == ring_buffer_size(rb));
	assert(0 == ring_buffer_read(rb, dst, 100));
	assert(900 == ring_buffer_size(rb));
	assert(0 != ring_buffer_read(rb, dst + 100, 901)); // underrun(hungry)
	assert(0 == ring_buffer_read(rb, dst + 100, 900));
	assert(0 == ring_buffer_size(rb));

	i = 1000;
	j = 1000;
	while (i < N)
	{
		n = 1000 - ring_buffer_size(rb);
		n = (N - i) > n ? n : (N - i);
		n = rand() % (n + 1);

		assert(0 == ring_buffer_write(rb, src + i, n));
		i += n;

		n = rand() % (ring_buffer_size(rb) + 1);
		assert(0 == ring_buffer_read(rb, dst + j, n));
		j += n;

		assert(j <= i);
	}

	n = ring_buffer_size(rb);
	assert(0 == ring_buffer_read(rb, dst + j, n));
	j += n;

	assert(0 == memcmp(src, dst, N));
	ring_buffer_destroy(rb);
	free(src);
	free(dst);
}
#endif
