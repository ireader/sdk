#include "channel.h"
#include "sys/sema.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

struct channel_t
{
	int elesize; // element size
	int capacity; // channel capacity(by element)
	int count, offset;
	uint8_t *ptr;

	locker_t locker;
    sema_t reader;
    sema_t writer;
};

struct channel_t* channel_create(int capacity, int elementsize)
{
	struct channel_t* c;
	assert(capacity > 0 && elementsize > 0);
	c = (struct channel_t*)malloc(sizeof(*c) + capacity * elementsize);
	if (c)
	{
		memset(c, 0, sizeof(*c));
		locker_create(&c->locker);
		sema_create(&c->reader, NULL, 0);
		sema_create(&c->writer, NULL, capacity);
		c->elesize = elementsize;
		c->capacity = capacity;
		c->ptr = (uint8_t*)(c + 1);
	}
	return c;
}

void channel_destroy(struct channel_t** pc)
{
	struct channel_t* c;

	if (!pc || !*pc)
		return;

	c = *pc;
	*pc = NULL;

	sema_destroy(&c->reader);
	sema_destroy(&c->writer);
	locker_destroy(&c->locker);
	free(c);
}

//void channel_clear(struct channel_t* c)
//{
//	locker_lock(&c->locker);
//	c->count = 0;
//	c->offset = 0;
//	locker_unlock(&c->locker);
//}

int channel_count(struct channel_t* c)
{
	// TODO: memory alignment
	return c->count;
}

int channel_push(struct channel_t* c, const void* e)
{
	sema_wait(&c->writer);
	
	locker_lock(&c->locker);
	assert(c->count < c->capacity);
	memcpy(c->ptr + ((c->offset + c->count) % c->capacity) * c->elesize, e, c->elesize);
	c->count += 1;
	locker_unlock(&c->locker);

	sema_post(&c->reader);
	return 0;
}

int channel_pop(struct channel_t* c, void* e)
{
	sema_wait(&c->reader);

	locker_lock(&c->locker);
	memcpy(e, c->ptr + c->offset * c->elesize, c->elesize);
	c->count -= 1;
	c->offset = (c->offset + 1) % c->capacity;
	assert(c->count >= 0);
	locker_unlock(&c->locker);

	sema_post(&c->writer);
	return 0;
}

int channel_push_timeout(struct channel_t* c, const void* e, int timeout)
{
	int r;
	r = sema_timewait(&c->writer, timeout);
	if (0 != r)
		return r;

	locker_lock(&c->locker);
	assert(c->count < c->capacity);
	memcpy(c->ptr + ((c->offset + c->count) % c->capacity) * c->elesize, e, c->elesize);
	c->count += 1;
	locker_unlock(&c->locker);

	sema_post(&c->reader);
	return 0;
}

int channel_pop_timeout(struct channel_t* c, void* e, int timeout)
{
	int r;
	r = sema_timewait(&c->reader, timeout);
	if (0 != r)
		return r;

	locker_lock(&c->locker);
	memcpy(e, c->ptr + c->offset * c->elesize, c->elesize);
	c->count -= 1;
	c->offset = (c->offset + 1) % c->capacity;
	assert(c->count >= 0);
	locker_unlock(&c->locker);

	sema_post(&c->writer);
	return 0;
}
