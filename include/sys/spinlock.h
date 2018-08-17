#ifndef _platform_spinlock_h_
#define _platform_spinlock_h_

#if defined(OS_WINDOWS)
#include <Windows.h>
typedef CRITICAL_SECTION spinlock_t;
#elif defined(OS_MAC)
#include <assert.h>
#include <libkern/OSAtomic.h>
typedef OSSpinLock spinlock_t;
#elif defined(OS_LINUX_KERNEL)
#include <linux/spinlock.h>
#else
#include <pthread.h>
typedef pthread_spinlock_t spinlock_t;
#endif

//----------------------------------------------
// int spinlock_create(spinlock_t *locker)
// int spinlock_destroy(spinlock_t *locker)
// void spinlock_lock(spinlock_t *locker)
// void spinlock_unlock(spinlock_t *locker)
// int spinlock_trylock(spinlock_t *locker)
//----------------------------------------------

static inline int spinlock_create(spinlock_t *locker)
{
#if defined(OS_WINDOWS)
	// Minimum support OS: WinXP
	return InitializeCriticalSectionAndSpinCount(locker, 0x00000400) ? 0: -1;
#elif defined(OS_MAC)
	// see more Apple Developer spinlock
	// OSSpinLock is an integer type.  The convention is that unlocked is zero, and locked is nonzero.  
	// Locks must be naturally aligned and cannot be in cache-inhibited memory.
#if TARGET_CPU_X86_64 || TARGET_CPU_PPC64
	assert((intptr_t)locker % 8 == 0);
#else
	assert((intptr_t)locker % 4 == 0);
#endif
	*locker = 0;
	return 0;
#elif defined(OS_LINUX_KERNEL)
	spin_lock_init(locker);
	return 0;
#else
	return pthread_spin_init(locker, PTHREAD_PROCESS_PRIVATE);
#endif
}

static inline int spinlock_destroy(spinlock_t *locker)
{
#if defined(OS_WINDOWS)
	DeleteCriticalSection(locker); return 0;
#elif defined(OS_MAC)
    (void)locker;
	return 0; // do nothing
#elif defined(OS_LINUX_KERNEL)
	return 0; // do nothing
#else
	return pthread_spin_destroy(locker);
#endif
}

static inline void spinlock_lock(spinlock_t *locker)
{
#if defined(OS_WINDOWS)
	EnterCriticalSection(locker);
#elif defined(OS_MAC)
	OSSpinLockLock(locker);
#elif defined(OS_LINUX_KERNEL)
	spin_lock(locker);
#else
	pthread_spin_lock(locker);
#endif
}

static inline void spinlock_unlock(spinlock_t *locker)
{
#if defined(OS_WINDOWS)
	LeaveCriticalSection(locker);
#elif defined(OS_MAC)
	OSSpinLockUnlock(locker);
#elif defined(OS_LINUX_KERNEL)
	spin_unlock(locker);
#else
	pthread_spin_unlock(locker);
#endif
}

static inline int spinlock_trylock(spinlock_t *locker)
{
#if defined(OS_WINDOWS)
	return TryEnterCriticalSection(locker) ? 1 : 0;
#elif defined(OS_MAC)
	return OSSpinLockTry(locker) ? 1 : 0;
#elif defined(OS_LINUX_KERNEL)
	return spin_trylock(locker);
#else
	return pthread_spin_trylock(locker);
#endif
}

#endif /* !_platform_spinlock_h_ */
