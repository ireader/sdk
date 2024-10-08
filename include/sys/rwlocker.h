#ifndef _platform_rwlocker_h_
#define _platform_rwlocker_h_

#include <errno.h>

#if defined(OS_WINDOWS)
#include <Windows.h>
typedef SRWLOCK	rwlocker_t;
#elif defined(OS_RTOS)
#include "sys/locker.h"
typedef locker_t rwlocker_t;
#else
#include <pthread.h>
typedef pthread_rwlock_t rwlocker_t;
#endif

//-------------------------------------------------------------------------------------
// int rwlocker_create(rwlocker_t* locker);
// int rwlocker_destroy(rwlocker_t* locker);
// int rwlocker_rdlock(rwlocker_t* locker); // Shared mode
// int rwlocker_wrlock(rwlocker_t* locker); // Exclusive mode
// int rwlocker_rdunlock(rwlocker_t* locker);
// int rwlocker_wrunlock(rwlocker_t* locker);
// int rwlocker_tryrdlock(rwlocker_t* locker);
// int rwlocker_trywrlock(rwlocker_t* locker);
//-------------------------------------------------------------------------------------

static inline int rwlocker_create(rwlocker_t* locker)
{
#if defined(OS_WINDOWS)
	InitializeSRWLock(locker); return 0;
#elif defined(OS_RTOS)
	return locker_create(locker);
#else
	//pthread_rwlockattr_t attr;
	//pthread_rwlockattr_init(&attr);
	//pthread_rwlockattr_destroy(&attr);
	return pthread_rwlock_init(locker, NULL);
#endif
}

static inline int rwlocker_destroy(rwlocker_t* locker)
{
#if defined(OS_WINDOWS)
	//MSDN: SRW locks do not need to be explicitly destroyed.
	(void)locker;
	return 0;
#elif defined(OS_RTOS)
	return locker_create(locker);
#else
	return pthread_rwlock_destroy(locker);
#endif
}

static inline int rwlocker_rdlock(rwlocker_t* locker)
{
#if defined(OS_WINDOWS)
	AcquireSRWLockShared(locker); return 0;
#elif defined(OS_RTOS)
	return locker_lock(locker);
#else
	return pthread_rwlock_rdlock(locker);
#endif
}

static inline int rwlocker_wrlock(rwlocker_t* locker)
{
#if defined(OS_WINDOWS)
	AcquireSRWLockExclusive(locker); return 0;
#elif defined(OS_RTOS)
	return locker_lock(locker);
#else
	return pthread_rwlock_wrlock(locker);
#endif
}

static inline int rwlocker_rdunlock(rwlocker_t* locker)
{
#if defined(OS_WINDOWS)
	ReleaseSRWLockShared(locker); return 0;
#elif defined(OS_RTOS)
	return locker_unlock(locker);
#else
	return pthread_rwlock_unlock(locker);
#endif
}

static inline int rwlocker_wrunlock(rwlocker_t* locker)
{
#if defined(OS_WINDOWS)
	ReleaseSRWLockExclusive(locker); return 0;
#elif defined(OS_RTOS)
	return locker_unlock(locker);
#else
	return pthread_rwlock_unlock(locker);
#endif
}

// @return 0-ok, other-error
static inline int rwlocker_tryrdlock(rwlocker_t* locker)
{
#if defined(OS_WINDOWS)
#if (_WIN32_WINNT >= 0x0601 /*win7*/ )
	return 0 == TryAcquireSRWLockShared(locker) ? -1 : 0;
#else
	return -1;
#endif
#elif defined(OS_RTOS)
	return locker_lock(locker);
#else
	return pthread_rwlock_tryrdlock(locker);
#endif
}

// @return 0-ok, other-error
static inline int rwlocker_trywrlock(rwlocker_t* locker)
{
#if defined(OS_WINDOWS)
#if (_WIN32_WINNT >= 0x0601 /*win7*/ )
	return 0 == TryAcquireSRWLockExclusive(locker) ? -1 : 0;
#else
	return -1;
#endif
#elif defined(OS_RTOS)
	return locker_trylock(locker);
#else
	return pthread_rwlock_trywrlock(locker);
#endif
}

#endif /* !_platform_rwlocker_h_ */
