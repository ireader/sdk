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

static struct time_wheel_t s_wheel;

static int timer_cascade(uint64_t clock, struct time_bucket_t* tv, int index);

int timer_init(uint64_t clock)
{
	s_wheel.clock = clock;
	return spinlock_create(&s_wheel.locker);
}

int timer_clean(void)
{
	return spinlock_destroy(&s_wheel.locker);
}

int timer_start(struct timer_t* timer, uint64_t clock)
{
	int i;
	uint64_t diff;
	struct time_bucket_t* tv;

	assert(timer->ontimeout);
	spinlock_lock(&s_wheel.locker);
	diff = TIME(timer->expire - clock); // per 64ms

	if (timer->expire < clock)
	{
		i = TIME(s_wheel.clock) & TVR_MASK;
		tv = s_wheel.tv1 + i;
	}
	else if (diff < (1 << TVR_BITS))
	{
		i = TIME(timer->expire) & TVR_MASK;
		tv = s_wheel.tv1 + i;
	}
	else if (diff < (1 << (TVR_BITS + TVN_BITS)))
	{
		i = (TIME(timer->expire) >> TVR_BITS) & TVN_MASK;
		tv = s_wheel.tv2 + i;
	}
	else if (diff < (1 << (TVR_BITS + 2 * TVN_BITS)))
	{
		i = (TIME(timer->expire) >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
		tv = s_wheel.tv3 + i;
	}
	else if (diff < (1 << (TVR_BITS + 3 * TVN_BITS)))
	{
		i = (TIME(timer->expire) >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK;
		tv = s_wheel.tv4 + i;
	}
	else if (diff < (1ULL << (TVR_BITS + 4 * TVN_BITS)))
	{
		i = (TIME(timer->expire) >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK;
		tv = s_wheel.tv5 + i;
	}
	else
	{
		spinlock_unlock(&s_wheel.locker);
		assert(0); // excede max timeout value
		return -1;
	}

	// list insert
	timer->pprev = &tv->first;
	timer->next = tv->first;
	if (timer->next)
		timer->next->pprev = &timer->next;
	tv->first = timer;
	spinlock_unlock(&s_wheel.locker);
	return 0;
}

void timer_stop(struct timer_t* timer)
{
	spinlock_lock(&s_wheel.locker);
	*timer->pprev = timer->next;
	if(timer->next)
		timer->next->pprev = timer->pprev;
	spinlock_unlock(&s_wheel.locker);
}

int timer_process(uint64_t clock)
{
	int index;
	struct timer_t* timer;
	struct time_bucket_t bucket;

	spinlock_lock(&s_wheel.locker);
	while(s_wheel.clock < clock)
	{
		index = (int)(TIME(s_wheel.clock) & TVR_MASK);

		if (0 == index 
			&& 0 == timer_cascade(clock, s_wheel.tv2, TVN_INDEX(clock, 0))
			&& 0 == timer_cascade(clock, s_wheel.tv3, TVN_INDEX(clock, 1))
			&& 0 == timer_cascade(clock, s_wheel.tv4, TVN_INDEX(clock, 2)))
		{
			timer_cascade(clock, s_wheel.tv5, TVN_INDEX(clock, 3));
		}

		// move bucket
		bucket.first = s_wheel.tv1[index].first;
		if (bucket.first)
			bucket.first->pprev = &bucket.first;
		s_wheel.tv1[index].first = NULL; // clear

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
				spinlock_unlock(&s_wheel.locker);
				timer->ontimeout(timer->param);
				spinlock_lock(&s_wheel.locker);
			}
		}

		s_wheel.clock += (1 << TIME_RESOLUTION);
	}

	spinlock_unlock(&s_wheel.locker);
	return (int)(s_wheel.clock - clock);
}

static int timer_cascade(uint64_t clock, struct time_bucket_t* tv, int index)
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
		timer_start(timer, clock);
	}

	return index;
}
