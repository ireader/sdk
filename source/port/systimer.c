#include "port/systimer.h"

// Linux link with -lrt

#if defined(OS_WINDOWS)
#include <Windows.h>
#pragma comment(lib, "Winmm.lib")
#define OS_WINDOWS_TIMER
#else
#include <signal.h>
#include <time.h>
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

typedef struct _timer_context_t
{
	systimer_proc callback;
	void* cbparam;

#if defined(OS_WINDOWS_TIMER)
	UINT timerId;
	unsigned int period;
	unsigned int count;
	LONG ref;
	CRITICAL_SECTION locker;
#elif defined(OS_WINDOWS_ASYNC)
	HANDLE timerId;
#elif defined(OS_LINUX)
	timer_t timerId;
	int ontshot;
#endif
} timer_context_t;

#if defined(OS_WINDOWS_TIMER)
struct 
{
	TIMECAPS tc;
	thread_pool_t pool;
} g_ctx;

#define TIMER_PERIOD 1000
#endif

#if defined(OS_WINDOWS_TIMER) || defined(OS_WINDOWS_ASYNC)
static void timer_destroy(timer_context_t* ctx)
{
	DeleteCriticalSection(&ctx->locker);
	free(ctx);
}

static void timer_thread_worker(void *param)
{
	timer_context_t* ctx;
	ctx = (timer_context_t*)param;
	EnterCriticalSection(&ctx->locker);
	if(0 != ctx->timerId)
	{
		if(ctx->period > g_ctx.tc.wPeriodMax)
		{
			if(ctx->count == ctx->period / TIMER_PERIOD)
				ctx->callback((systimer_t)ctx, ctx->cbparam);
			ctx->count = (ctx->count + 1) % (ctx->period / TIMER_PERIOD + 1);
		}
		else
		{
			ctx->callback((systimer_t)ctx, ctx->cbparam);
		}
	}
	LeaveCriticalSection(&ctx->locker);

	if(0 == InterlockedDecrement(&ctx->ref))
		timer_destroy(ctx);
}

static void CALLBACK timer_schd_worker(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	timer_context_t* ctx;
	ctx = (timer_context_t*)dwUser;
	if(2 != InterlockedIncrement(&ctx->ref))
	{
		InterlockedDecrement(&ctx->ref); // make sure only one callback
		assert(ctx->ref > 0);
	}
	else
	{
		// one timer only can be call in one thread
		thread_pool_push(g_ctx.pool, timer_thread_worker, ctx);
	}
}
#elif defined(OS_LINUX)
static void timer_schd_worker(union sigval v)
{
	timer_context_t* ctx;
	ctx = (timer_context_t*)v.sival_ptr;
	ctx->callback((systimer_t)ctx, ctx->cbparam);
}
#else
static int timer_schd_worker(void *param)
{
}
#endif

int systimer_init(thread_pool_t pool)
{
#if defined(OS_WINDOWS_TIMER)
	timeGetDevCaps(&g_ctx.tc, sizeof(TIMECAPS));
	g_ctx.pool = pool;
#endif
	return 0;
}

int systimer_clean(void)
{
	return 0;
}

#if defined(OS_WINDOWS_TIMER)
static int systimer_create(systimer_t* id, unsigned int period, int oneshot, systimer_proc callback, void* cbparam)
{
	UINT fuEvent;
	timer_context_t* ctx;

	if(oneshot && g_ctx.tc.wPeriodMin > period && period > g_ctx.tc.wPeriodMax)
		return -EINVAL;

	ctx = (timer_context_t*)malloc(sizeof(timer_context_t));
	if(!ctx)
		return -ENOMEM;

	memset(ctx, 0, sizeof(timer_context_t));
	InitializeCriticalSection(&ctx->locker);
	ctx->callback = callback;
	ctx->cbparam = cbparam;
	ctx->period = period;
	ctx->count = 0;
	ctx->ref = 1;

	// check period value
	period = (period > g_ctx.tc.wPeriodMax) ?  TIMER_PERIOD : period;
	fuEvent = (oneshot?TIME_ONESHOT:TIME_PERIODIC)|TIME_CALLBACK_FUNCTION;
	ctx->timerId = timeSetEvent(period, 10, timer_schd_worker, (DWORD_PTR)ctx, fuEvent);
	if(0 == ctx->timerId)
	{
		timer_destroy(ctx);
		return -EINVAL;
	}

	*id = (systimer_t)ctx;
	return 0;
}
#elif defined(OS_LINUX)
static int systimer_create(systimer_t* id, unsigned int period, int oneshot, systimer_proc callback, void* cbparam)
{
	struct sigevent sev;
	struct itimerspec tv;
	timer_context_t* ctx;

	ctx = (timer_context_t*)malloc(sizeof(timer_context_t));
	if(!ctx)
		return -ENOMEM;;

	memset(ctx, 0, sizeof(timer_context_t));
	ctx->callback = callback;
	ctx->cbparam = cbparam;

	memset(&sev, 0, sizeof(sev));
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_value.sival_ptr = ctx;
	sev.sigev_notify_function = timer_schd_worker;
	if(0 != timer_create(CLOCK_MONOTONIC, &sev, &ctx->timerId))
	{
		free(ctx);
		return -errno;
	}

	tv.it_interval.tv_sec = period / 1000;
	tv.it_interval.tv_nsec = (period % 1000) * 1000000; // 10(-9)second
	tv.it_value.tv_sec = tv.it_interval.tv_sec;
	tv.it_value.tv_nsec = tv.it_interval.tv_nsec;
	if(0 != timer_settime(ctx->timerId, 0, &tv, NULL))
	{
		timer_delete(ctx->timerId);
		free(ctx);
		return -errno;
	}

	*id = (systimer_t)ctx;
	return 0;
}
#elif defined(OS_WINDOWS_ASYNC)
static int systimer_create(systimer_t* id, unsigned int period, int oneshot, systimer_proc callback, void* cbparam)
{
	LARGE_INTEGER tv;
	timer_context_t* ctx;
	ctx = (timer_context_t*)malloc(sizeof(timer_context_t));
	if(!ctx)
		return -ENOMEM;

	memset(ctx, 0, sizeof(timer_context_t));
	ctx->callback = callback;
	ctx->cbparam = cbparam;

	ctx->timerId = CreateWaitableTimer(NULL, FALSE, NULL);
	if(0 == ctx->timerId)
	{
		free(ctx);
		return -(int)GetLastError();
	}

	tv.QuadPart = -10000L * period; // in 100 nanosecond intervals
	if(!SetWaitableTimer(ctx->timerId, &tv, oneshot?0:period, timer_schd_worker, ctx, FALSE))
	{
		CloseHandle(ctx->timerId);
		free(ctx);
		return -(int)GetLastError();
	}

	*id = (systimer_t)ctx;
	return 0;
}
#else
static int systimer_create(systimer_t* id, int period, systimer_proc callback, void* cbparam)
{
	ERROR: dont implemention
}
#endif

int systimer_oneshot(systimer_t *id, unsigned int period, systimer_proc callback, void* cbparam)
{
	return systimer_create(id, period, 1, callback, cbparam);
}

int systimer_start(systimer_t* id, unsigned int period, systimer_proc callback, void* cbparam)
{
	return systimer_create(id, period, 0, callback, cbparam);
}

int systimer_stop(systimer_t id)
{
	timer_context_t* ctx;
	if(!id) return -EINVAL;

	ctx = (timer_context_t*)id;

#if defined(OS_WINDOWS_TIMER)
	timeKillEvent(ctx->timerId);
	EnterCriticalSection(&ctx->locker);
	ctx->timerId = 0;
	LeaveCriticalSection(&ctx->locker);
	if(0 == InterlockedDecrement(&ctx->ref))
		timer_destroy(ctx);
	return 0;
#elif defined(OS_LINUX)
	timer_delete(ctx->timerId);
#elif defined(OS_WINDOWS_ASYNC)
	CloseHandle(ctx->timerId);
#else
	ERROR: dont implemention
#endif

	free(ctx); // take care call-back function
	return 0;
}
