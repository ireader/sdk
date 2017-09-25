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
	int i;
	struct timer_t* timer;

	timer = (struct timer_t*)calloc(N, sizeof(*timer));
	assert(timer);

	now = time(NULL);
	srand((int)now);
	timer_init(now);

	for(i = 0; i < N; i++)
	{
		timer[i].ontimeout = ontimer;
		timer[i].param = &timer[i];
		timer[i].expire = now + rand();
		timer_start(&timer[i], now);

		timer_process(now);
		now += rand() % 256;
	}

	while (count > 0)
	{
		timer_process(now);
		now += rand() % 256;
	}

	free(timer);
	printf("timer test ok.\n");
}
