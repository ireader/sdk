#include "twtimer.h"
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#define TIMER 1000
#define TOTAL 10000000
#define TIMER_RESOLUTION 64

static uint64_t now;
static time_wheel_t* wheel;
static struct twtimer_t* s_timer;
static int s_started = 0;
static int s_stoped = 0;
static int s_cancel = 0;

static void ontimer(void* param)
{
	struct twtimer_t* timer = (struct twtimer_t*)param;
	assert(timer->expire / TIMER_RESOLUTION <= now / TIMER_RESOLUTION);
	timer->param = NULL;
	++s_stoped;
}

static void timer_delete(void)
{
	struct twtimer_t* t;
	t = &s_timer[now % TIMER];
	if (t->param)
	{
		twtimer_stop(wheel, t);
		t->param = NULL;
		++s_stoped;
		++s_cancel;
	}
}

void timer_test(void)
{
	int i;

	s_timer = (struct twtimer_t*)calloc(TIMER, sizeof(*s_timer));
	assert(s_timer);

	now = time(NULL);
	srand((int)now);
	wheel = time_wheel_create(now);

	while (s_stoped < TOTAL)
	{
		for(i = 0; i < TIMER && s_started < TOTAL; i++)
		{
			if (s_timer[i].param)
				continue;
			
			++s_started;
			s_timer[i].ontimeout = ontimer;
			s_timer[i].param = &s_timer[i];
			s_timer[i].expire = now + rand() % 4096;
			twtimer_start(wheel, &s_timer[i]);
		}

		twtimer_process(wheel, now);
		now += rand() % 256;

		timer_delete();
	}

	free(s_timer);
	time_wheel_destroy(wheel);
	printf("timer(total: %d, cancel: %d) test ok.\n", TOTAL, s_cancel);
}
