#include "timer.h"
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#define N 100000

static uint64_t now;
static int count = N;

static void ontimer(void* param)
{
	//struct timer_t* timer = (struct timer_t*)param;
	//printf("[%d] expire: %llu, clock: %llu\n", N-count, timer->expire, now);
	--count;
}

void timer_test(void)
{
	int i, v;
	time_wheel_t* wheel;
	struct timer_t* timer;

	timer = (struct timer_t*)calloc(N, sizeof(*timer));
	assert(timer);

	now = time(NULL);
	srand((int)now);
	wheel = time_wheel_create(now);

	for(i = 0; i < N; i++)
	{
		timer[i].ontimeout = ontimer;
		timer[i].param = &timer[i];
		timer[i].expire = now + rand() % 4096;
		timer_start(wheel, &timer[i], now);

		timer_process(wheel, now);
		now += rand() % 256;
	}

	while (count > 0)
	{
		timer_process(wheel, now);
		now += rand() % 256;
	}

	free(timer);
	time_wheel_destroy(wheel);
	printf("timer test ok.\n");
}
