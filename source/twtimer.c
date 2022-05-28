// Timing Wheel Timer(timeout)
// 64ms per bucket
// http://www.cs.columbia.edu/~nahum/w6998/papers/sosp87-timing-wheels.pdf

#include "twtimer.h"
#include "sys/spinlock.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#define TIME_RESOLUTION 3 // (0xFFFFFFFF << 3) / (24 * 3600 * 1000) ~= 397day
#define TIME(clock) ((clock) >> TIME_RESOLUTION) // per 8ms

#define TVR_BITS 8
#define TVN_BITS 6
#define TVR_SIZE (1 << TVR_BITS)
#define TVN_SIZE (1 << TVN_BITS)
#define TVR_MASK (TVR_SIZE - 1)
#define TVN_MASK (TVN_SIZE - 1)

#define TVR_INDEX(clock)    ((int)((clock >> TIME_RESOLUTION) & TVR_MASK))
#define TVN_INDEX(clock, n) ((int)((clock >> (TIME_RESOLUTION + TVR_BITS + (n * TVN_BITS))) & TVN_MASK))

struct time_bucket_t
{
	struct twtimer_t* first;
};

struct time_wheel_t
{
	spinlock_t locker;

	uint64_t count;
	uint64_t clock;
	struct time_bucket_t tv1[TVR_SIZE];
	struct time_bucket_t tv2[TVN_SIZE];
	struct time_bucket_t tv3[TVN_SIZE];
	struct time_bucket_t tv4[TVN_SIZE];
	struct time_bucket_t tv5[TVN_SIZE];
};

static int twtimer_add(struct time_wheel_t* tm, struct twtimer_t* timer);
static int twtimer_cascade(struct time_wheel_t* tm, struct time_bucket_t* tv, int index);

struct time_wheel_t* time_wheel_create(uint64_t clock)
{
	struct time_wheel_t* tm;
	tm = (struct time_wheel_t*)calloc(1, sizeof(*tm));
	if (tm)
	{
		tm->count = 0;
		tm->clock = clock;
		spinlock_create(&tm->locker);
	}
	return tm;
}

int time_wheel_destroy(struct time_wheel_t* tm)
{
	assert(0 == tm->count);
	return spinlock_destroy(&tm->locker);
}

int twtimer_start(struct time_wheel_t* tm, struct twtimer_t* timer)
{
	int r;
	assert(timer->ontimeout);
	spinlock_lock(&tm->locker);
	r = twtimer_add(tm, timer);
	spinlock_unlock(&tm->locker);
	return r;
}

int twtimer_stop(struct time_wheel_t* tm, struct twtimer_t* timer)
{
	struct twtimer_t** pprev;
	spinlock_lock(&tm->locker);
	pprev = timer->pprev;
	if (timer->pprev)
	{
		--tm->count; // timer validation ???
		*timer->pprev = timer->next;
	}
	if (timer->next)
		timer->next->pprev = timer->pprev;
	timer->pprev = NULL;
	timer->next = NULL;
	spinlock_unlock(&tm->locker);
	return pprev ? 0 : -1;
}

int twtimer_process(struct time_wheel_t* tm, uint64_t clock)
{
	int index;
	struct twtimer_t* timer;
    struct time_bucket_t bucket;

	spinlock_lock(&tm->locker);
	while(TIME(tm->clock) < TIME(clock))
	{
		index = TVR_INDEX(tm->clock);

		if (0 == index 
			&& 0 == twtimer_cascade(tm, tm->tv2, TVN_INDEX(tm->clock, 0))
			&& 0 == twtimer_cascade(tm, tm->tv3, TVN_INDEX(tm->clock, 1))
			&& 0 == twtimer_cascade(tm, tm->tv4, TVN_INDEX(tm->clock, 2)))
		{
			twtimer_cascade(tm, tm->tv5, TVN_INDEX(tm->clock, 3));
		}

		// move bucket
		bucket.first = tm->tv1[index].first;
		tm->tv1[index].first = NULL; // clear
		tm->clock += (1 << TIME_RESOLUTION);

		// trigger timer
        while (bucket.first)
		{
            timer = bucket.first;
			if (timer->next)
				timer->next->pprev = &bucket.first;
            bucket.first = timer->next;
            
			timer->next = NULL;
			timer->pprev = NULL;
			--tm->count;
			if (timer->ontimeout)
			{
				spinlock_unlock(&tm->locker);
                //assert(timer->expire >= clock - 2 * (1<<TIME_RESOLUTION));
                //assert(timer->expire <= clock + 2 * (1<<TIME_RESOLUTION));
				timer->ontimeout(timer->param);
				spinlock_lock(&tm->locker);
			}
		}	
    }

	spinlock_unlock(&tm->locker);
	return (int)(tm->clock - clock);
}

static int twtimer_cascade(struct time_wheel_t* tm, struct time_bucket_t* tv, int index)
{
	struct twtimer_t* timer;
	struct twtimer_t* next;
	next = tv[index].first;
	tv[index].first = NULL; // clear

	for (timer = next; timer; timer = next)
	{
		--tm->count; // start will add count
		next = timer->next;
		timer->next = NULL;
		timer->pprev = NULL;
		twtimer_add(tm, timer);
	}

	return index;
}

static int twtimer_add(struct time_wheel_t* tm, struct twtimer_t* timer)
{
	uint64_t diff;
	struct time_bucket_t* tv;

	assert(timer->ontimeout);
	if (timer->pprev)
	{
		assert(0); // timer have been started
		return -EEXIST;
	}

	diff = TIME(timer->expire - tm->clock); // per 64ms

	if (timer->expire < tm->clock)
	{
		tv = tm->tv1 + TVR_INDEX(tm->clock);
	}
	else if (diff < (1 << TVR_BITS))
	{
		tv = tm->tv1 + TVR_INDEX(timer->expire);
	}
	else if (diff < (1 << (TVR_BITS + TVN_BITS)))
	{
		tv = tm->tv2 + TVN_INDEX(timer->expire, 0);
	}
	else if (diff < (1 << (TVR_BITS + 2 * TVN_BITS)))
	{
		tv = tm->tv3 + TVN_INDEX(timer->expire, 1);
	}
	else if (diff < (1 << (TVR_BITS + 3 * TVN_BITS)))
	{
		tv = tm->tv4 + TVN_INDEX(timer->expire, 2);
	}
	else if (diff < (1ULL << (TVR_BITS + 4 * TVN_BITS)))
	{
		tv = tm->tv5 + TVN_INDEX(timer->expire, 3);
	}
	else
	{
		assert(0); // exceed max timeout value
		return -1;
	}

	// list insert
	timer->pprev = &tv->first;
	timer->next = tv->first;
	if (timer->next)
		timer->next->pprev = &timer->next;
	tv->first = timer;
	++tm->count;
	return 0;
}
