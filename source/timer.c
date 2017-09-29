// Timing Wheel Timer(timeout)
// 64ms per bucket
// http://www.cs.columbia.edu/~nahum/w6998/papers/sosp87-timing-wheels.pdf

#include "timer.h"
#include "sys/spinlock.h"
#include <stdint.h>
#include <assert.h>

#define TIME_RESOLUTION 6
#define TIME(clock) ((clock) >> TIME_RESOLUTION) // per 64ms

#define TVR_BITS 8
#define TVN_BITS 6
#define TVR_SIZE (1 << TVR_BITS)
#define TVN_SIZE (1 << TVN_BITS)
#define TVR_MASK (TVR_SIZE - 1)
#define TVN_MASK (TVN_SIZE - 1)

#define TVN_INDEX(clock, n) ((int)((clock >> (TIME_RESOLUTION + TVR_BITS + (n * TVN_BITS))) & TVN_MASK))

struct time_bucket_t
{
	struct timer_t* first;
};

struct time_wheel_t
{
	spinlock_t locker;

	uint64_t clock;
	struct time_bucket_t tv1[TVR_SIZE];
	struct time_bucket_t tv2[TVN_SIZE];
	struct time_bucket_t tv3[TVN_SIZE];
	struct time_bucket_t tv4[TVN_SIZE];
	struct time_bucket_t tv5[TVN_SIZE];
};

static int timer_cascade(struct time_wheel_t* tm, uint64_t clock, struct time_bucket_t* tv, int index);

struct time_wheel_t* time_wheel_create(uint64_t clock)
{
	struct time_wheel_t* tm;
	tm = (struct time_wheel_t*)calloc(1, sizeof(*tm));
	if (tm)
	{
		tm->clock = clock;
		spinlock_create(&tm->locker);
	}
	return tm;
}

int time_wheel_destroy(struct time_wheel_t* tm)
{
	return spinlock_destroy(&tm->locker);
}

int timer_start(struct time_wheel_t* tm, struct timer_t* timer, uint64_t clock)
{
	int i;
	uint64_t diff;
	struct time_bucket_t* tv;

	assert(timer->ontimeout);
	spinlock_lock(&tm->locker);
	diff = TIME(timer->expire - clock); // per 64ms

	if (timer->expire < clock)
	{
		i = TIME(tm->clock) & TVR_MASK;
		tv = tm->tv1 + i;
	}
	else if (diff < (1 << TVR_BITS))
	{
		i = TIME(timer->expire) & TVR_MASK;
		tv = tm->tv1 + i;
	}
	else if (diff < (1 << (TVR_BITS + TVN_BITS)))
	{
		i = (TIME(timer->expire) >> TVR_BITS) & TVN_MASK;
		tv = tm->tv2 + i;
	}
	else if (diff < (1 << (TVR_BITS + 2 * TVN_BITS)))
	{
		i = (TIME(timer->expire) >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
		tv = tm->tv3 + i;
	}
	else if (diff < (1 << (TVR_BITS + 3 * TVN_BITS)))
	{
		i = (TIME(timer->expire) >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK;
		tv = tm->tv4 + i;
	}
	else if (diff < (1ULL << (TVR_BITS + 4 * TVN_BITS)))
	{
		i = (TIME(timer->expire) >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK;
		tv = tm->tv5 + i;
	}
	else
	{
		spinlock_unlock(&tm->locker);
		assert(0); // excede max timeout value
		return -1;
	}

	// list insert
	timer->pprev = &tv->first;
	timer->next = tv->first;
	if (timer->next)
		timer->next->pprev = &timer->next;
	tv->first = timer;
	spinlock_unlock(&tm->locker);
	return 0;
}

void timer_stop(struct time_wheel_t* tm, struct timer_t* timer)
{
	spinlock_lock(&tm->locker);
	*timer->pprev = timer->next;
	if(timer->next)
		timer->next->pprev = timer->pprev;
	spinlock_unlock(&tm->locker);
}

int timer_process(struct time_wheel_t* tm, uint64_t clock)
{
	int index;
	struct timer_t* timer;
	struct time_bucket_t bucket;

	spinlock_lock(&tm->locker);
	while(tm->clock < clock)
	{
		index = (int)(TIME(tm->clock) & TVR_MASK);

		if (0 == index 
			&& 0 == timer_cascade(tm, clock, tm->tv2, TVN_INDEX(clock, 0))
			&& 0 == timer_cascade(tm, clock, tm->tv3, TVN_INDEX(clock, 1))
			&& 0 == timer_cascade(tm, clock, tm->tv4, TVN_INDEX(clock, 2)))
		{
			timer_cascade(tm, clock, tm->tv5, TVN_INDEX(clock, 3));
		}

		// move bucket
		bucket.first = tm->tv1[index].first;
		if (bucket.first)
			bucket.first->pprev = &bucket.first;
		tm->tv1[index].first = NULL; // clear

		// trigger timer
		while (bucket.first)
		{
			timer = bucket.first;
			bucket.first = timer->next;
			if (timer->next)
				timer->pprev = &bucket.first;
			timer->next = NULL;
			timer->pprev = NULL;
			if (timer->ontimeout)
			{
				spinlock_unlock(&tm->locker);
				timer->ontimeout(timer->param);
				spinlock_lock(&tm->locker);
			}
		}

		tm->clock += (1 << TIME_RESOLUTION);
	}

	spinlock_unlock(&tm->locker);
	return (int)(tm->clock - clock);
}

static int timer_cascade(struct time_wheel_t* tm, uint64_t clock, struct time_bucket_t* tv, int index)
{
	struct timer_t* timer, *next;
	struct time_bucket_t bucket;
	bucket.first = tv[index].first;
	if (bucket.first)
		bucket.first->pprev = &bucket.first;
	tv[index].first = NULL; // clear

	for (timer = bucket.first; timer; timer = next)
	{
		next = timer->next;
		timer_start(tm, timer, clock);
	}

	return index;
}
