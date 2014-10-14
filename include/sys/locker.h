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

inline int locker_create(locker_t* locker)
{
#if defined(OS_WINDOWS)
	InitializeCriticalSection(locker);
	return 0;
#else
	// create a recusive locker
	int r;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	//pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	r = pthread_mutex_init(locker, &attr);
	pthread_mutexattr_destroy(&attr);
	return r;
#endif
}

inline int locker_destroy(locker_t* locker)
{
#if defined(OS_WINDOWS)
	DeleteCriticalSection(locker);
	return 0;
#else
	return pthread_mutex_destroy(locker);
#endif
}

inline int locker_lock(locker_t* locker)
{
#if defined(OS_WINDOWS)
	EnterCriticalSection(locker);
	return 0;
#else
	return pthread_mutex_lock(locker);
#endif
}

// linux: unlock thread must is the lock thread
inline int locker_unlock(locker_t* locker)
{
#if defined(OS_WINDOWS)
	LeaveCriticalSection(locker);
	return 0;
#else
	return pthread_mutex_unlock(locker);
#endif
}

inline int locker_trylock(locker_t* locker)
{
#if defined(OS_WINDOWS)
	return TryEnterCriticalSection(locker)?0:-1;
#else
	return pthread_mutex_trylock(locker);
#endif
}

#endif /* !_platform_locker_h_ */
