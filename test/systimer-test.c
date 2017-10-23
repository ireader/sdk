#include "cstringext.h"
#include "port/systimer.h"
#include "port/system.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "time64.h"
#include <stdio.h>
#include <time.h>

static void OnTimer(systimer_t id, void* param)
{
#if defined(OS_LINUX)
	printf("[%p]timer: %d\n", thread_self(), (int)param);
#else
	printf("[%u]timer: %d\n", (unsigned int)thread_getid(thread_self()), (int)param);
#endif
}

static void Test1(void)
{
	systimer_t id;
	assert(0 == systimer_start(&id, 3*1000, OnTimer, (void*)10000));
	system_sleep(10000);
	assert(0 == systimer_stop(id));
}

static void OnTest2(systimer_t id, void* param)
{
	printf("[%u]Test2: enter timer %d\n", (unsigned int)thread_getid(thread_self()), (int)param);
	system_sleep(10000);
	printf("[%u]Test2: leave timer %d\n", (unsigned int)thread_getid(thread_self()), (int)param);
}

static void Test2(void)
{
	systimer_t id;
	assert(0 == systimer_start(&id, 1000, OnTest2, (void*)20000));
	system_sleep(2000);
	printf("Test2 before stop timer.\n");
	assert(0 == systimer_stop(id));
	printf("Test2 after stop timer.\n");
}

static void Test3(void)
{
	systimer_t id[2];
	assert(0 == systimer_start(&id[0], 500, OnTimer, (void*)300001));
	assert(0 == systimer_start(&id[1], 1000, OnTimer, (void*)300002));
	system_sleep(1000*1000);
	assert(0 == systimer_stop(id[0]));
	assert(0 == systimer_stop(id[1]));
}

static void Test4(void)
{
	int i;
	systimer_t id[1000];

	for(i=0; i<sizeof(id)/sizeof(id[0]); i++)
	{
		assert(0 == systimer_start(&id[i], 1000, OnTimer, (void*)(40000+i)));
	}

	for(i=0; i<sizeof(id)/sizeof(id[0]); i++)
	{
		assert(0 == systimer_stop(id[i]));
	}
}

static void OnClockTimer(systimer_t id, void* param)
{
	char time[24];
	//printf("OnClockTimer tp.tv_sec: %u, tp.tv_nsec: %ld\n", tp.tv_sec, tp.tv_nsec);
}

static void systimer_settimeofday(void)
{
	systimer_t id;
	size_t t1, t2;
	time64_t lt;
	char time[24];
	
	lt = time64_now();
	systimer_start(&id, 10000, OnTimer, (void*)NULL);

	system_sleep(1000);
	lt = time64_now();

	t2 = system_clock();
	system_sleep(15000);
	systimer_stop(id);
}

#if defined(OS_LINUX)
static void OnClockTimer2(systimer_t id, void* param)
{
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	printf("OnClockTimer tp.tv_sec: %s, tp.tv_nsec: %ld\n", ctime(tp.tv_sec), tp.tv_nsec);
}

static void systimer_clocksettime(void)
{
	systimer_t id;
	struct timespec tp;

	clock_gettime(CLOCK_REALTIME, &tp);
	printf("systimer_clocksettime tp.tv_sec: %s, tp.tv_nsec: %ld\n", ctime(tp.tv_sec), tp.tv_nsec);

	systimer_start(&id, 10000, OnClockTimer2, (void*)NULL);

	system_sleep(1000);
	clock_gettime(CLOCK_REALTIME, &tp);
	tp.tv_sec += 5;
	clock_settime(CLOCK_REALTIME, &tp);

	system_sleep(15000);
	systimer_stop(id);
}
#endif

void systimer_test(void)
{
	thread_pool_t tpool = thread_pool_create(2, 2, 64);
	systimer_init(tpool);
	Test1();
	Test2();
	Test3();
	Test4();
#if defined(OS_LINUX)
	systimer_clocksettime();
#endif
	systimer_clean();
	thread_pool_destroy(tpool);
}
