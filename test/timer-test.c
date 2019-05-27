#include "twtimer.h"
#include "sys/atomic.h"
#include "sys/thread.h"
#include "sys/system.h"
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#define TIMER 0x3FFFFF
#define TIMER_RESOLUTION 16
#define WORKER 3

static int s_cancel = 0;

static void ontimer1(void* param)
{
    int* counter = (int*)param;
    *counter += 1;
}

static void timer_check_cascade(time_wheel_t* wheel, struct twtimer_t* timers, uint64_t now)
{
    int i, counter;
    for(i = 0; i < TIMER; i++)
    {
        timers[i].ontimeout = ontimer1;
        timers[i].param = &counter;
        timers[i].expire = now + (i << 4);
        twtimer_start(wheel, &timers[i]);
    }
    
    for(counter = i = 0; i < TIMER + 1; i++)
    {
        twtimer_process(wheel, now + (i << 4));
        assert(counter == i);
    }
    assert(counter == TIMER);
}

static int STDCALL timer_worker(void* param)
{
    struct time_wheel_t* tw;
    tw = (struct time_wheel_t*)param;

    while(0 == s_cancel)
    {
        twtimer_process(tw, system_clock());
        system_sleep(10);
    }
    
    return 0;
}

static void ontimer2(void* param)
{
}

static void timer_check_remove(time_wheel_t* wheel, struct twtimer_t* timers, uint64_t now)
{
    int i, j;
    pthread_t worker[WORKER];
    
    s_cancel = 0;
    for(i = 0; i < WORKER; i++)
        thread_create(&worker[i], timer_worker, wheel);
    
    for(j = 0; j < 100; j++)
    {
        for(i = 0; i < TIMER; i++)
        {
            timers[i].ontimeout = ontimer2;
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
}

void timer_test(void)
{
    uint64_t now;
    time_wheel_t* wheel;
    struct twtimer_t* timers;
    
    now = system_clock();
    srand((unsigned int)now);
    wheel = time_wheel_create(now);
	timers = (struct twtimer_t*)calloc(TIMER, sizeof(struct twtimer_t));

    // 1. test cascade
    timer_check_cascade(wheel, timers, now);
    
    // 2. dynamic add/delete timer
    timer_check_remove(wheel, timers, now);
    
	time_wheel_destroy(wheel);
    free(timers);
    printf("timer test ok.\n");
}
