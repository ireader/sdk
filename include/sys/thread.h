#ifndef _platform_thread_h_
#define _platform_thread_h_

#if defined(OS_WINDOWS)
#include <Windows.h>
#include <process.h>

#ifndef STDCALL
#define STDCALL __stdcall
#endif

typedef struct
{
	DWORD id;
	HANDLE handle;
} pthread_t;

typedef DWORD tid_t;

#else
#include <pthread.h>
#include <sched.h>

typedef pthread_t tid_t;

#ifndef STDCALL
#define STDCALL
#endif

enum thread_priority
{
	THREAD_PRIORITY_IDLE			= 1,
	THREAD_PRIORITY_LOWEST			= 25,
	THREAD_PRIORITY_BELOW_NORMAL	= 40,
	THREAD_PRIORITY_NORMAL			= 50,
	THREAD_PRIORITY_ABOVE_NORMAL	= 60,
	THREAD_PRIORITY_HIGHEST			= 75,
	THREAD_PRIORITY_TIME_CRITICAL	= 99,
};

#endif

//-------------------------------------------------------------------------------------
// int thread_create(pthread_t* thread, thread_proc func, void* param);
// int thread_destroy(pthread_t thread);
// int thread_detach(pthread_t thread);
// int thread_getpriority(pthread_t thread, int* priority);
// int thread_setpriority(pthread_t thread, int priority);
// int thread_getid(pthread_t thread, tid_t* id);
// int thread_isself(pthread_t thread);
// int thread_valid(pthread_t thread);
// tid_t thread_self(void);
// int thread_yield(void);
//-------------------------------------------------------------------------------------

typedef int (STDCALL *thread_proc)(void* param);

static inline int thread_create(pthread_t* thread, thread_proc func, void* param)
{
#if defined(OS_WINDOWS)
	// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682453.aspx
	// CreateThread function: By default, every thread has one megabyte of stack space. 

	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms682453%28v=vs.85%29.aspx
	// A thread in an executable that calls the C run-time library (CRT) 
	// should use the _beginthreadex and _endthreadex functions for thread management 
	// rather than CreateThread and ExitThread;

	//thread->handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, param, 0, &thread->id);
	typedef unsigned int (__stdcall *thread_routine)(void *);
	thread->handle = (HANDLE)_beginthreadex(NULL, 0, (thread_routine)func, param, 0, (unsigned int*)&thread->id);
	if(NULL == thread->handle)
		return -1;
	return 0;
#else
	typedef void* (*linux_thread_routine)(void*);
	return pthread_create(thread, NULL, (linux_thread_routine)func, param);
#endif
}

static inline int thread_destroy(pthread_t thread)
{
#if defined(OS_WINDOWS)
	if(thread.id != GetCurrentThreadId())
		WaitForSingleObjectEx(thread.handle, INFINITE, TRUE);
	CloseHandle(thread.handle);
	return 0;
#else
	void* value = NULL;
	if(pthread_equal(pthread_self(),thread))
        return pthread_detach(thread);
	else
        return pthread_join(thread, &value);
#endif
}

static inline int thread_detach(pthread_t thread)
{
#if defined(OS_WINDOWS)
	CloseHandle(thread.handle);
	return 0;
#else
	return pthread_detach(thread);
#endif
}

// priority: [-15, 15]
// 0: normal / -15: idle / 15: critical
static inline int thread_getpriority(pthread_t thread, int* priority)
{
#if defined(OS_WINDOWS)
	int r = GetThreadPriority(thread.handle);
	if(THREAD_PRIORITY_ERROR_RETURN == r)
		return (int)GetLastError();

	*priority = r;
	return 0;
#else
	int policy;
	struct sched_param sched;
	int r = pthread_getschedparam(thread, &policy, &sched);
	if(0 == r)
		*priority = sched.sched_priority;
	return r;
#endif
}

static inline int thread_setpriority(pthread_t thread, int priority)
{
#if defined(OS_WINDOWS)
	BOOL r = SetThreadPriority(thread.handle, priority);
	return TRUE==r?1:0;
#else
	int policy = SCHED_RR;
	struct sched_param sched;
	pthread_getschedparam(thread, &policy, &sched);

	// For processes scheduled under one of the normal scheduling policies 
	// (SCHED_OTHER, SCHED_IDLE, SCHED_BATCH), 
	// sched_priority is not used in scheduling decisions (it must be specified as 0).
	// Processes scheduled under one of the real-time policies(SCHED_FIFO, SCHED_RR) 
	// have a sched_priority value in the range 1 (low)to 99 (high)
	sched.sched_priority = (SCHED_FIFO==policy || SCHED_RR==policy) ? priority : 0;
	return pthread_setschedparam(thread, policy, &sched);
#endif
}

static inline tid_t thread_self(void)
{
#if defined(OS_WINDOWS)
	DWORD id = GetCurrentThreadId();
	return id;
#else
	return pthread_self();
#endif
}

static inline int thread_getid(pthread_t thread, tid_t* id)
{
#if defined(OS_WINDOWS)
	*id = thread.id;
	//DWORD tid = GetThreadId(thread.handle); // >= vista
	//if(0 == tid)
	//	return GetLastError();
	//*id = (int)tid;
	return 0;
#else
	*id = thread;
	return 0;
#endif
}

static inline int thread_isself(pthread_t thread)
{
#if defined(OS_WINDOWS)
	return thread.id==GetCurrentThreadId() ? 1 : 0;
#else
	return pthread_equal(pthread_self(), thread);
#endif
}

static inline int thread_valid(pthread_t thread)
{
#if defined(OS_WINDOWS)
	return 0 != thread.id ? 1 : 0;
#else
	return 0 != thread ? 1 : 0;
#endif
}

static inline int thread_yield(void)
{
#if defined(OS_WINDOWS)
	// Causes the calling thread to yield execution to another thread that is ready to run 
	// on the current processor. The operating system selects the next thread to be executed.
	return (int)SwitchToThread();
#else
	return sched_yield();
#endif
}

#endif /* !_platform_thread_h_ */
