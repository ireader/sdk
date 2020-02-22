#include "twtimer.h"
#include "sys/atomic.h"
#include "sys/thread.h"
#include "sys/system.h"
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define TIMER 0x3FFFFF
#define TIMER_RESOLUTION 3
#define WORKER 3

struct timer_test_t
{
	int running;
	uint64_t clock;
	time_wheel_t* wheel;
	struct twtimer_t* timer;
};

static int s_cancel = 0;

static void ontimer1(void* param)
{
    int* counter = (int*)param;
    *counter += 1;
}

static void timer_check_cascade()
{
	uint64_t now;
    int i, counter;
	time_wheel_t* wheel;
	struct twtimer_t* timers;

	now = system_clock();
	wheel = time_wheel_create(now);
	timers = (struct twtimer_t*)calloc(TIMER, sizeof(struct twtimer_t));

    for(i = 0; i < TIMER; i++)
    {
        timers[i].ontimeout = ontimer1;
        timers[i].param = &counter;
        timers[i].expire = now + (i << TIMER_RESOLUTION);
        twtimer_start(wheel, &timers[i]);
    }
    
    for(counter = i = 0; i < TIMER + 1; i++)
    {
        twtimer_process(wheel, now + (i << TIMER_RESOLUTION));
        assert(counter == i);
    }
    assert(counter == TIMER);

	time_wheel_destroy(wheel);
	free(timers);
}

static void timer_check_cascade2()
{
	uint64_t now;
	int i, counter;
	time_wheel_t* wheel;
	struct twtimer_t* timers;

	now = system_clock();
	wheel = time_wheel_create(now);
	timers = (struct twtimer_t*)calloc(TIMER, sizeof(struct twtimer_t));

	for (i = 0; i < TIMER; i++)
	{
		timers[i].ontimeout = ontimer1;
		timers[i].param = &counter;
		timers[i].expire = now + (i << TIMER_RESOLUTION);
		twtimer_start(wheel, &timers[i]);
	}

	for (counter = i = 0; i < TIMER + 1; i+=rand()%(TIMER+1))
	{
		twtimer_process(wheel, now + (i << TIMER_RESOLUTION));
		assert(counter == i);
	}
	twtimer_process(wheel, now + (i << TIMER_RESOLUTION));
	assert(counter == TIMER);

	time_wheel_destroy(wheel);
	free(timers);
}

static void timer_check3()
{
	uint64_t now;
	int counter;
	time_wheel_t* wheel;
	struct twtimer_t timer;

	now = system_clock();
	wheel = time_wheel_create(now);
	
	counter = 0;
	memset(&timer, 0, sizeof(timer));
	timer.ontimeout = ontimer1;
	timer.param = &counter;
	timer.expire = now;
	twtimer_start(wheel, &timer);
	twtimer_process(wheel, now + (1 << TIMER_RESOLUTION));
	assert(1 == counter);

	memset(&timer, 0, sizeof(timer));
	timer.ontimeout = ontimer1;
	timer.param = &counter;
	timer.expire = now;
	twtimer_start(wheel, &timer);
	twtimer_process(wheel, now + (2 << TIMER_RESOLUTION));
	assert(2 == counter);

	time_wheel_destroy(wheel);
}

static int STDCALL timer_worker(void* param)
{
	struct time_wheel_t* tw;
	tw = (struct time_wheel_t*)param;

	while (0 == s_cancel)
	{
		twtimer_process(tw, system_clock());
		system_sleep(10);
	}

	return 0;
}


static void ontimer2(void* param)
{
	static int lastclock;
	struct timer_test_t* t;
	t = (struct timer_test_t*)param;
	if (t->running)
	{
		t->timer->expire += 5000;
		twtimer_start((time_wheel_t*)t->wheel, t->timer);
	}

	if (0 != lastclock)
		assert(t->clock - lastclock > 4000);
	lastclock = t->clock;
}

static void timer_check_addmanytimes()
{
	int i;
	struct timer_test_t t;
	struct twtimer_t timer;

	t.running = 1;
	t.timer = &timer;
	t.clock = system_clock();
	t.wheel = time_wheel_create(t.clock);
	memset(&timer, 0, sizeof(timer));
	timer.ontimeout = ontimer2;
	timer.param = &t;
	timer.expire = t.clock + 5000;
	twtimer_start(t.wheel, &timer);
	
	for (i = 0; i < 30 * 5000; i++)
	{
		t.clock++;
		twtimer_process(t.wheel, t.clock);
	}

	// clear all
	t.running = 0;
	t.clock += 10000;
	twtimer_process(t.wheel, t.clock);
	time_wheel_destroy(t.wheel);
}

static void ontimer3(void* param)
{
}

static void timer_check_remove()
{
    int i, j;
	uint64_t now;
	time_wheel_t* wheel;
	struct twtimer_t* timers;
	pthread_t worker[WORKER];

	now = system_clock();
	wheel = time_wheel_create(now);
	timers = (struct twtimer_t*)calloc(TIMER, sizeof(struct twtimer_t));
    
    s_cancel = 0;
    for(i = 0; i < WORKER; i++)
        thread_create(&worker[i], timer_worker, wheel);
    
    for(j = 0; j < 100; j++)
    {
        for(i = 0; i < TIMER; i++)
        {
            timers[i].ontimeout = ontimer3;
            timers[i].param = NULL;
            timers[i].expire = now + rand() % 4096;
            twtimer_start(wheel, &timers[i]);
        }
        
        system_sleep(10);
        for(i = 0; i < TIMER; i++)
            twtimer_stop(wheel, &timers[i]);
    }
    
    s_cancel = 1;
    for(i = 0; i < WORKER; i++)
        thread_destroy(worker[i]);

	time_wheel_destroy(wheel);
	free(timers);
}

void timer_test(void)
{
    srand((unsigned int)system_clock());

	timer_check_addmanytimes();
    timer_check_cascade();
	timer_check_cascade2();
    timer_check3();
	timer_check_remove();

    printf("timer test ok.\n");
}
