#include "timer.h"
#include "cstringext.h"
#include "sys/process.h"
#include "sys/system.h"
#include "sys/sync.h"
#include "thread-pool.h"

#if defined(OS_WINDOWS)
#pragma comment(lib, "Winmm.lib")
#else
#include <sys/timerfd.h>
#endif

#include <map>
#include <assert.h>
#include <stdlib.h>

class TimerManager
{
public:
	TimerManager();
	~TimerManager();

public:
	int AddTimer(timer_t& id, int period, fcbTimer callback, void* param);
	int DelTimer(timer_t id);
	int SetPeriod(timer_t id, int period);
	int GetPeriod(timer_t id, int& period);

private:
	static int ThreadProc(void* param);

private:
	struct TimerItem
	{
		timer_t _id;		
		int		_period;
		void*	_param;
		fcbTimer _callback;
		thread_t _thread;
		double	_time;	// last time
	};
	typedef std::map<timer_t, TimerItem*> TTimers;
	TTimers m_timers;
	ThreadLocker m_locker;
};

TimerManager::TimerManager()
{
}

TimerManager::~TimerManager()
{
	for(TTimers::iterator it=m_timers.begin(); it!=m_timers.end(); ++it)
	{
		TimerItem* item = it->second;
		free(item);
	}
}

int TimerManager::AddTimer(timer_t &id, int period, fcbTimer callback, void *param)
{
	assert(period > 0 && callback);
	AutoThreadLocker locker(m_locker);
	TimerItem* item = (TimerItem*)malloc(sizeof(TimerItem));
	memset(item, 0, sizeof(item));
	item->_id = (timer_t)item;
	item->_period = period;
	item->_param = param;
	item->_callback = callback;
	item->_time = 0.0;
	//item->_thread = 0;

	std::pair<TTimers::iterator, bool> pr = m_timers.insert(std::make_pair(item->_id, item));
	if(!pr.second)
	{
		free(item);
		return -1;
	}

	int r = thread_create(&item->_thread, ThreadProc, item);
	if(r < 0)
	{
		free(item);
		m_timers.erase(pr.first);
	}
	else
	{
		id = item->_id;
	}
	return r;
}

int TimerManager::DelTimer(timer_t id)
{
	AutoThreadLocker locker(m_locker);
	TTimers::iterator it = m_timers.find(id);
	if(it == m_timers.end())
		return -1;

	TimerItem* item = it->second;
	assert(item);
	assert(thread_valid(item->_thread));
	item->_id = 0; // notify thread exit
	thread_destroy(item->_thread);
	free(item);
	m_timers.erase(it);
	return 0;
}

int TimerManager::SetPeriod(timer_t id, int period)
{
	assert(period > 0);
	AutoThreadLocker locker(m_locker);
	TTimers::iterator it = m_timers.find(id);
	if(it == m_timers.end())
		return -1;

	TimerItem* item = it->second;
	assert(item);
	item->_period = period;
	return 0;
}

int TimerManager::GetPeriod(timer_t id, int &period)
{
	AutoThreadLocker locker(m_locker);
	TTimers::iterator it = m_timers.find(id);
	if(it == m_timers.end())
		return -1;

	TimerItem* item = it->second;
	assert(item);
	period = item->_period;
	return 0;
}

int TimerManager::ThreadProc(void *param)
{
	//TimerManager* self = (TimerManager*)&g_timermgr;
	TimerItem* item = (TimerItem*)param;
	assert(item);

	while(0 != item->_id) // DelTimer changed item->id
	{
		double ft = system_time();
		if(0.0==item->_time || int((ft-item->_time)*1000) >= item->_period)
		{
			item->_callback(item->_id, item->_param);
			item->_time = ft;
		}
		else
		{
			system_sleep(1000);
		}
	}
	return 0;
}

TimerManager& FetchTimerMgr()
{
	static TimerManager s_timermgr;
	return s_timermgr;
}

struct _timer_list_t
{
	struct _timer_list_t *prev;
	struct _timer_list_t *next;
};

typedef struct _timer_context_t
{
	struct _timer_list_t *list;
	long locked;
	timer_func callback;
	void* cbparam;

#if defined(OS_WINDOWS)
	UINT timerId;
#elif defined(OS_LINUX)
	int timerId;
#endif
} timer_context_t;

typedef struct _timer_schd_context_t
{
	locker_t locker;
	thread_pool_t pool;

	struct _timer_list_t head;
} timer_schd_context_t;

static timer_schd_context_t g_sched;

#if defined(OS_WINDOWS)
static void timer_callback(void *param)
{
	timer_context_t* ctx;
	ctx = (timer_context_t*)param;
	ctx->callback(ctx->timerId, ctx->cbparam);
	InterlockedDecrement(&ctx->locked); // unlock
}

static void timer_schd_worker(UINT /*uTimerID*/, UINT /*uMsg*/, DWORD_PTR dwUser, DWORD_PTR /*dw1*/, DWORD_PTR /*dw2*/)
{
	timer_context_t* ctx;
	ctx = (timer_context_t*)dwUser;
	if(1 == InterlockedIncrement(&ctx->locked)) // lock
	{
		thread_pool_push(g_sched.pool, timer_callback, ctx);
	}
	else
	{
		InterlockedDecrement(&ctx->locked);
	}
}
#elif defined(OS_LINUX)
static int timer_schd_worker(void *param)
{
}
#else
static int timer_schd_worker(void *param)
{
}
#endif

static int timer_schd_link(timer_context_t *ctx)
{
#if defined(OS_LINUX)
	// notify
#endif
}

static int timer_schd_unlink(timer_context_t *ctx)
{
#if defined(OS_LINUX)
	// notify
#endif
}

int timer_init()
{
	g_sched.pool = thread_pool_create(1, 1, system_getcpucount()*2);
}

int timer_cleanup()
{
	thread_pool_destroy(g_sched.pool);
}

#if defined(OS_WINDOWS)
int timer_add(int period, timer_func callback, void* cbparam)
{
	timer_context_t* ctx;
	ctx = (timer_context_t*)malloc(sizeof(timer_context_t));
	if(!ctx)
		return -1;

	memset(ctx, 0, sizeof(timer_context_t));
	ctx->timerId = timeSetEvent(period, (period+1)/2, timer_schd_worker, ctx, TIME_PERIODIC|TIME_CALLBACK_FUNCTION);
	if(NULL == ctx->timerId)
	{
		free(ctx);
		return -1;
	}

	timer_schd_link(ctx);
	*id = (timer_t)ctx;
	return 0;
}
#elif defined(OS_LINUX)
int timer_add(timer_t* id, int period, timer_func callback, void* cbparam)
{
	struct itimerspec tv;
	timer_context_t* ctx;
	ctx = (timer_context_t*)malloc(sizeof(timer_context_t));
	if(!ctx)
		return -1;

	memset(ctx, 0, sizeof(timer_context_t));
	ctx->timerId = timerfd_create(CLOCK_MONOTONIC, 0);
	if(-1 == ctx->timerId)
	{
		free(ctx);
		return -1;
	}

	tv.it_interval.tv_sec = period / 1000;
	tv.it_interval.tv_nsec = (period % 1000) * 1000000; // 10(-9)second
	tv.it_value.tv_sec = (time_t)(-1); // never expired
	tv.it_value.tv_nsec = 0;
	if(0 != timerfd_settime(ctx->timerId, 0, &tv, NULL))
	{
		close(ctx->timerId);
		free(ctx);
		return -1;
	}

	timer_schd_link(ctx);
	*id = (timer_t)ctx;
	return 0;
}
#else
int timer_add(timer_t* id, int period, timer_func callback, void* cbparam)
{
}
#endif

int timer_remove(timer_t *id)
{
	timer_context_t* ctx;
	if(!id || !*id) return 0;

	ctx = (timer_context_t*)*id;

	// wait for timer done
	while(1 != InterlockedIncrement(&ctx->locked))
	{
		InterlockedDecrement(&ctx->locked);
		system_sleep(1);
	}

#if defined(OS_WINDOWS)
	timeKillEvent(ctx->timerId);
#elif defined(OS_LINUX)
	close(ctx->timerId);
#else
#endif

	timer_schd_unlink(ctx);
	free(ctx);
	*id = NULL;
	return 0;
}
