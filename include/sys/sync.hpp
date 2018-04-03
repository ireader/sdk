#ifndef _platform_sync_hpp_
#define _platform_sync_hpp_

#include "locker.h"
#include "event.h"
#include "sema.h"
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
///
/// locker
///
//////////////////////////////////////////////////////////////////////////
class ThreadLocker
{
	ThreadLocker(const ThreadLocker&){}
	ThreadLocker& operator =(const ThreadLocker&){ return *this; }

public:
	ThreadLocker()
	{
		locker_create(&m_locker);
	}

	~ThreadLocker()
	{
		locker_destroy(&m_locker);
	}

	int Lock()
	{
		return locker_lock(&m_locker);
	}

	int Unlock()
	{
		return locker_unlock(&m_locker);
	}

	int Trylock()
	{
		return locker_trylock(&m_locker);
	}

private:
	locker_t m_locker;
};

class AutoThreadLocker
{
	AutoThreadLocker(const AutoThreadLocker& locker):m_locker(locker.m_locker){}
	AutoThreadLocker& operator =(const AutoThreadLocker&){ return *this; }

public:
	AutoThreadLocker(ThreadLocker& locker)
		:m_locker(locker)
	{
		m_locker.Lock();
	}

	~AutoThreadLocker()
	{
		m_locker.Unlock();
	}

private:
	ThreadLocker& m_locker;
};

//////////////////////////////////////////////////////////////////////////
///
/// event
///
//////////////////////////////////////////////////////////////////////////
class ThreadEvent
{
	ThreadEvent(const ThreadEvent&){}
	ThreadEvent& operator =(const ThreadEvent&){ return *this; }

public:
	ThreadEvent()
	{
		event_create(&m_event);
	}

	~ThreadEvent()
	{
		event_destroy(&m_event);
	}

	int Wait()
	{
		return event_wait(&m_event);
	}

	int TimeWait(int timeout)
	{
		return event_timewait(&m_event, timeout);
	}

	int Signal()
	{
		return event_signal(&m_event);
	}

	int Reset()
	{
		return event_reset(&m_event);
	}

private:
	event_t m_event;
};

//////////////////////////////////////////////////////////////////////////
///
/// semaphore
///
//////////////////////////////////////////////////////////////////////////
class CSemaphore
{
	CSemaphore(const CSemaphore&){}
	CSemaphore& operator =(const CSemaphore&){ return *this; }

public:
	CSemaphore(int initValue)
	{
		m_err = sema_create(&m_sema, NULL, initValue);
	}

	CSemaphore(const char* name)
	{
		m_err = Open(name);
	}

	~CSemaphore()
	{
		sema_destroy(&m_sema);
	}

	int Post()
	{
		return sema_post(&m_sema);
	}

	int Wait()
	{
		return sema_wait(&m_sema);
	}

	int TimeWait(int timeout)
	{
		return sema_timewait(&m_sema, timeout);
	}

	int Trywait()
	{
		return sema_trywait(&m_sema);
	}

	int CheckError() const
	{
		return m_err;
	}

private:
	int Open(const char* name)
	{
		int r = sema_open(&m_sema, name);
		if (0 != r)
		{
			r = sema_create(&m_sema, name, 1);
			if (0 == r)
			{
				r = sema_open(&m_sema, name);
			}
		}
		return r;
	}

private:
	int m_err;
	sema_t m_sema;
};

#endif /* !_platform_sync_hpp_ */
