#ifndef _platform_locker_h_
#define _platform_locker_h_

#include <errno.h>

#if defined(OS_WINDOWS)
#include <Windows.h>
typedef CRITICAL_SECTION	locker_t;
#else
#include <pthread.h>
typedef pthread_mutex_t		locker_t;
#endif

//-------------------------------------------------------------------------------------
// int locker_create(locker_t* locker);
// int locker_destroy(locker_t* locker);
// int locker_lock(locker_t* locker);
// int locker_unlock(locker_t* locker);
// int locker_trylock(locker_t* locker);
//-------------------------------------------------------------------------------------

static inline int locker_create(locker_t* locker)
{
#if defined(OS_WINDOWS)
	InitializeCriticalSection(locker);
	return 0;
#else
	// create a recusive locker
	int r;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	// http://linux.die.net/man/3/pthread_mutexattr_settype
	// Application Usage:
	// It is advised that an application should not use a PTHREAD_MUTEX_RECURSIVE mutex 
	// with condition variables because the implicit unlock performed for a pthread_cond_timedwait() 
	// or pthread_cond_wait() may not actually release the mutex (if it had been locked multiple times). 
	// If this happens, no other thread can satisfy the condition of the predicate. 
#if defined(OS_LINUX)
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
	//pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
#else
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	//pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
#endif
	r = pthread_mutex_init(locker, &attr);
	pthread_mutexattr_destroy(&attr);
	return r;
#endif
}

static inline int locker_destroy(locker_t* locker)
{
#if defined(OS_WINDOWS)
	DeleteCriticalSection(locker);
	return 0;
#else
	return pthread_mutex_destroy(locker);
#endif
}

static inline int locker_lock(locker_t* locker)
{
#if defined(OS_WINDOWS)
	EnterCriticalSection(locker);
	return 0;
#else
    // These functions shall not return an error code of [EINTR].
	return pthread_mutex_lock(locker);
#endif
}

// linux: unlock thread must is the lock thread
static inline int locker_unlock(locker_t* locker)
{
#if defined(OS_WINDOWS)
	LeaveCriticalSection(locker);
	return 0;
#else
	return pthread_mutex_unlock(locker);
#endif
}

static inline int locker_trylock(locker_t* locker)
{
#if defined(OS_WINDOWS)
	return TryEnterCriticalSection(locker)?0:-1;
#else
	return pthread_mutex_trylock(locker);
#endif
}

#endif /* !_platform_locker_h_ */
