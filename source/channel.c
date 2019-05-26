#include "channel.h"
#include "sys/sema.h"
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
    int nreaders;
    int nwriters;
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
		sema_create(&c->writer, NULL, 0);
		c->elesize = elementsize;
		c->capacity = capacity;
		c->ptr = (uint8_t*)(c + 1);
	}
	return c;
}

void channel_destroy(struct channel_t** pq)
{
	struct channel_t* c;

	if (!pq || !*pq)
		return;

	c = *pq;
	*pq = NULL;

	assert(0 == c->nreaders && 0 == c->nwriters);
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

static int channel_wait(struct channel_t* c, sema_t *sema, int* count)
{
	int r;

    assert(*count >= 0);
	*count += 1;
    locker_unlock(&c->locker);

	r = sema_wait(sema);
    assert(0 == r);
    
	locker_lock(&c->locker);
    return r;
}

static int channel_wakeup(struct channel_t* c, sema_t *sema, int* count)
{
    int r;
    assert(*count >= 0);
	if (*count < 1)
		return 0;

    *count -= 1;
    r = sema_post(sema);
    assert(0 == r);
    return r;
}

int channel_push(struct channel_t* c, const void* e)
{
	locker_lock(&c->locker);
    
    assert(c->nwriters >= 0 && c->count <= c->capacity);
	while (c->count >= c->capacity)
        channel_wait(c, &c->writer, &c->nwriters);

	memcpy(c->ptr + ((c->offset + c->count) % c->capacity) * c->elesize, e, c->elesize);
	c->count += 1;
	channel_wakeup(c, &c->reader, &c->nreaders);
	locker_unlock(&c->locker);
	return 0;
}

int channel_pop(struct channel_t* c, void* e)
{
	locker_lock(&c->locker);
    
    assert(c->nreaders >= 0 && c->count >= 0);
	while (0 == c->count)
        channel_wait(c, &c->reader, &c->nreaders);

	memcpy(e, c->ptr + c->offset * c->elesize, c->elesize);
	c->count -= 1;
	c->offset = (c->offset + 1) % c->capacity;
	channel_wakeup(c, &c->writer, &c->nwriters);
	locker_unlock(&c->locker);
	return 0;
}
