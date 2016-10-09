#include "timer.h"
#include <map>
#include <assert.h>
#include <stdlib.h>
#include "sys/thread.h"
#include "sys/sync.hpp"
#include "sys/system.h"

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
	static int STDCALL ThreadProc(void* param);

private:
	struct TimerItem
	{
		timer_t _id;		
		int		_period;
		void*	_param;
		fcbTimer _callback;
		pthread_t _thread;
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
	if(!item) return -1;
	memset(item, 0, sizeof(*item));
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

	assert(it->second);
	TimerItem* item = it->second;
	if(!item) return -1;
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
	if(item)
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
	if(item)
		period = item->_period;
	return 0;
}

int STDCALL TimerManager::ThreadProc(void *param)
{
	assert(param);
	//TimerManager* self = (TimerManager*)&g_timermgr;
	TimerItem* item = (TimerItem*)param;
	if(!item) return -1;

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

//////////////////////////////////////////////////////////////////////////
///
/// timer
///
//////////////////////////////////////////////////////////////////////////

int timer_create(timer_t* id, int period, fcbTimer callback, void* param)
{
	if(NULL==id)
		return -1;
	return FetchTimerMgr().AddTimer(*id, period, callback, param);
}

int timer_destroy(timer_t id)
{
	return FetchTimerMgr().DelTimer(id);
}

int timer_setperiod(timer_t id, int period)
{
	return FetchTimerMgr().SetPeriod(id, period);
}

int timer_getperiod(timer_t id, int* period)
{
	if(NULL == period)
		return -1;
	return FetchTimerMgr().GetPeriod(id, *period);
}
