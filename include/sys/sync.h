#ifndef _platform_sync_h_
#define _platform_sync_h_

#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>

typedef CRITICAL_SECTION	locker_t;
typedef HANDLE				event_t;
typedef HANDLE				semaphore_t;

#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <limits.h>
#include <sched.h>
#include <semaphore.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <time.h>

typedef pthread_mutex_t		locker_t;

typedef struct
{
	int count; // fixed pthread_cond_signal/pthread_cond_wait call order
	pthread_cond_t event;
	pthread_mutex_t mutex;
} event_t;

typedef struct
{
	sem_t*	semaphore;
	char	name[256];
} semaphore_t;

#ifndef WAIT_TIMEOUT
	#define WAIT_TIMEOUT		ETIMEDOUT
#endif

#endif

#ifndef IN
#define IN 
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

#if !defined(OS_WINDOWS)

#ifndef InterlockedIncrement
	#define InterlockedIncrement atomic_increment
#endif

#ifndef InterlockedDecrement
	#define InterlockedDecrement atomic_decrement
#endif

#endif

//////////////////////////////////////////////////////////////////////////
///
/// locker: Windows CriticalSection/Linux mutex
///	multi-processor: no
///
//////////////////////////////////////////////////////////////////////////
inline int locker_create(IN locker_t* locker);
inline int locker_destroy(IN locker_t* locker);
inline int locker_lock(IN locker_t* locker);
inline int locker_unlock(IN locker_t* locker); // linux: unlock thread must is the lock thread
inline int locker_trylock(IN locker_t* locker);


//////////////////////////////////////////////////////////////////////////
///
/// event: Windows Event/Linux condition variable
///	multi-processor: no
///
//////////////////////////////////////////////////////////////////////////
inline int event_create(IN event_t* event);
inline int event_destroy(IN event_t* event);
// 0-success, other-error
inline int event_wait(IN event_t* event);
// 0-success, WAIT_TIMEOUT-timeout, other-error
inline int event_timewait(IN event_t* event, IN int timeout);
inline int event_signal(IN event_t* event);
inline int event_reset(IN event_t* event);

//////////////////////////////////////////////////////////////////////////
///
/// semaphore:
/// Named semaphores(process-shared semaphore)/Unnamed semaphores(thread-shared semaphore)
///	multi-processor: yes
///
//////////////////////////////////////////////////////////////////////////
// Windows: the name can contain any character except the backslash
// 0-success, other-error
inline int semaphore_create(IN semaphore_t* semaphore, IN const char* name, IN long value);
// 0-success, other-error
inline int semaphore_open(IN semaphore_t* semaphore, IN const char* name);
// 0-success, other-error
inline int semaphore_wait(IN semaphore_t* semaphore);
// 0-success, WAIT_TIMEOUT-timeout, other-error
inline int semaphore_timewait(IN semaphore_t* semaphore, IN int timeout);
// 0-success, other-error
inline int semaphore_trywait(IN semaphore_t* semaphore);
// 0-success, other-error
inline int semaphore_post(IN semaphore_t* semaphore);
// 0-success, other-error
inline int semaphore_destroy(IN semaphore_t* semaphore);


#if defined(OS_WINDOWS)
// 1-success, other-error
inline int atomic_cas(INOUT long volatile *d, long c, long v)
{
	return c == InterlockedCompareExchange(d, v, c) ? 1 : 0;
}

#else

#if defined(__gcc__builtin_) && __GNUC__>=4 && __GNUC_MINOR__>=3
inline long atomic_add(INOUT long volatile *v, long incr)
{
	return __sync_fetch_and_add(v, incr);
}

inline int atomic_cas(INOUT long volatile *d, long c, long v)
{
	return __sync_bool_compare_and_swap(d, c, v) ? 1 : 0;
}

#elif defined(__ARM__) || defined(__arm__)
inline long atomic_add(INOUT long volatile *v, long incr)
{
	int a, b, c;
	asm volatile(	"0:\n\t"
					"ldr %0, [%3]\n\t"
					"add %1, %0, %4\n\t"
					"swp %2, %1, [%3]\n\t"
					"cmp %0, %2\n\t"
					"swpne %1, %2, [%3]\n\t"
					"bne 0b"
					: "=&r" (a), "=&r" (b), "=&r" (c)
					: "r" (v), "r" (incr)
					: "cc", "memory");
	return a;
}

#else
inline long atomic_add(INOUT long volatile *v, long incr)
{
	asm volatile ("lock; xaddl %k0,%1"
		: "=r" (incr), "=m" (*v)
		: "0" (incr), "m" (*v)
		: "memory", "cc");
	return incr;
}
#endif

inline long atomic_increment(INOUT volatile long* v)
{
	return atomic_add(v, 1)+1;
}

inline long atomic_decrement(INOUT volatile long* v)
{
	return atomic_add(v, -1)-1;
}
#endif

//////////////////////////////////////////////////////////////////////////
///
/// locker: Windows CriticalSection/Linux mutex
/// 
//////////////////////////////////////////////////////////////////////////
inline int locker_create(IN locker_t* locker)
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
	r = pthread_mutex_init(locker, &attr);
	pthread_mutexattr_destroy(&attr);
	return r;
#endif
}

inline int  locker_destroy(IN locker_t* locker)
{
#if defined(OS_WINDOWS)
	DeleteCriticalSection(locker);
	return 0;
#else
	return pthread_mutex_destroy(locker);
#endif
}

inline int locker_lock(IN locker_t* locker)
{
#if defined(OS_WINDOWS)
	EnterCriticalSection(locker);
	return 0;
#else
	return pthread_mutex_lock(locker);
#endif
}

inline int locker_unlock(IN locker_t* locker)
{
#if defined(OS_WINDOWS)
	LeaveCriticalSection(locker);
	return 0;
#else
	return pthread_mutex_unlock(locker);
#endif
}

inline int locker_trylock(IN locker_t* locker)
{
#if defined(OS_WINDOWS)
	return TryEnterCriticalSection(locker)?0:-1;
#else
	return pthread_mutex_trylock(locker);
#endif
}

//////////////////////////////////////////////////////////////////////////
///
/// event: Windows Event/Linux condition variable
/// 
//////////////////////////////////////////////////////////////////////////
inline int event_create(IN event_t* event)
{
#if defined(OS_WINDOWS)
	HANDLE h = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(NULL==h)
		return (int)GetLastError();
	*event = h;
	return 0;
#else
	int r;
	pthread_condattr_t attr;
	pthread_condattr_init(&attr);
#ifdef __USE_XOPEN2K
	pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
#endif

	event->count = 0;
	pthread_mutex_init(&event->mutex, NULL);
	r = pthread_cond_init(&event->event, &attr);
	pthread_condattr_destroy(&attr);
	return r;
#endif
}

inline int event_destroy(IN event_t* event)
{
#if defined(OS_WINDOWS)
	BOOL r = CloseHandle(*event);
	return r?0:(int)GetLastError();
#else
	int r = pthread_cond_destroy(&event->event);
	while(EBUSY == r)
	{
		usleep(1000);
		r = pthread_cond_destroy(&event->event);
	}
	pthread_mutex_destroy(&event->mutex);
	return r;
#endif
}

// 0-success, other-error
inline int event_wait(IN event_t* event)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*event, INFINITE, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#else
	int r = 0;
	pthread_mutex_lock(&event->mutex);
	if(0 == event->count)
		r = pthread_cond_wait(&event->event, &event->mutex);
	event->count = 0;
	pthread_mutex_unlock(&event->mutex);
	return r;
#endif
}

// 0-success, WAIT_TIMEOUT-timeout, other-error
inline int event_timewait(IN event_t* event, IN int timeout)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*event, timeout, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#else
#if defined(OS_MAC)
	int r = 0;
	struct timeval tv;
	struct timespec ts;
	gettimeofday(&tv, NULL);
	ts.tv_sec = tv.tv_sec + timeout/1000;
	ts.tv_nsec = tv.tv_usec * 1000 + (timeout%1000)*1000000;
#else
	int r = 0;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &t);
	ts.tv_sec += timeout/1000;
	ts.tv_nsec += (timeout%1000)*1000000;
#endif

	pthread_mutex_lock(&event->mutex);
	if(0 == event->count)
		r = pthread_cond_timedwait(&event->event, &event->mutex, &ts);
	event->count = 0;
	pthread_mutex_unlock(&event->mutex);
	return r;
#endif
}

inline int event_signal(IN event_t* event)
{
#if defined(OS_WINDOWS)
	return SetEvent(*event)?0:(int)GetLastError();
#else
	int r;
	pthread_mutex_lock(&event->mutex);
	event->count = 1;
	r = pthread_cond_signal(&event->event);
	pthread_mutex_unlock(&event->mutex);
	return r;
#endif
}

inline int event_reset(IN event_t* event)
{
#if defined(OS_WINDOWS)
	return ResetEvent(*event)?0:(int)GetLastError();
#else
	pthread_mutex_lock(&event->mutex);
	event->count = 0;
	pthread_mutex_unlock(&event->mutex);
	return 0;
#endif
}

//////////////////////////////////////////////////////////////////////////
///
/// named semaphore
///
//////////////////////////////////////////////////////////////////////////
inline int semaphore_create(IN semaphore_t* semaphore, IN const char* name, IN long value)
{
#if defined(OS_WINDOWS)
	HANDLE handle = CreateSemaphoreA(NULL, value, 0x7FFFFFFF, name);
	if(NULL == handle)
		return GetLastError();
	*semaphore = handle;
	return 0;
#else
	memset(semaphore->name, 0, sizeof(semaphore->name));
	if(name && *name)
	{
		// Named semaphores(process-shared semaphore)
		semaphore->semaphore = sem_open(name, O_CREAT|O_EXCL|O_RDWR, 0777, value);
		if(SEM_FAILED == semaphore->semaphore)
			return errno;
		strncpy(semaphore->name, name, sizeof(semaphore->name)-1);
		return 0;
	}
	else
	{
		// Unnamed semaphores (memory-based semaphores, thread-shared semaphore)
		semaphore->semaphore = (sem_t*)malloc(sizeof(sem_t));
		if(!semaphore->semaphore)
			return ENOMEM;
		return 0==sem_init(semaphore->semaphore, 0, value) ? 0 : errno;
	}
#endif
}

// 0-success, other-error
inline int semaphore_open(IN semaphore_t* semaphore, IN const char* name)
{
	if(!name || !*name)
		return EINVAL;

#if defined(OS_WINDOWS)
	*semaphore = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, name);
	return *semaphore ? 0 : GetLastError();
#else
	semaphore->semaphore = sem_open(name, 0);
	if(SEM_FAILED == semaphore->semaphore)
		return errno;

	memset(semaphore->name, 0, sizeof(semaphore->name));
	return 0;
#endif
}

// 0-success, other-error
inline int semaphore_wait(IN semaphore_t* semaphore)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*semaphore, INFINITE, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#else
	return sem_wait(semaphore->semaphore);
#endif
}

// 0-success, WAIT_TIMEOUT-timeout, other-error
inline int semaphore_timewait(IN semaphore_t* semaphore, IN int timeout)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*semaphore, timeout, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#else
#if defined(OS_MAC)
	struct timeval tv;
	struct timespec ts;
	gettimeofday(&tv, NULL);
	ts.tv_sec = tv.tv_sec + timeout/1000;
	ts.tv_nsec = tv.tv_usec * 1000 + (timeout%1000)*1000000;
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += timeout/1000;
	ts.tv_nsec += (timeout%1000)*1000000;
#endif
	return sem_timedwait(semaphore->semaphore, &ts);
#endif
}

// 0-success, other-error
inline int semaphore_trywait(IN semaphore_t* semaphore)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*semaphore, 0, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#else
	return sem_trywait(semaphore->semaphore);
#endif
}

// 0-success, other-error
inline int semaphore_post(IN semaphore_t* semaphore)
{
#if defined(OS_WINDOWS)
	long r;
	return ReleaseSemaphore(*semaphore, 1, &r)?0:GetLastError();
#else
	return sem_post(semaphore->semaphore);
#endif
}

// 0-success, other-error
inline int semaphore_destroy(IN semaphore_t* semaphore)
{
#if defined(OS_WINDOWS)
	return CloseHandle(*semaphore)?0:GetLastError();
#else
	int r;
	if(semaphore->name[0])
	{
		// sem_open
		r = sem_close(semaphore->semaphore);
		if(0 == r)
			r = sem_unlink(semaphore->name);
	}
	else
	{
		// sem_init
		r = sem_destroy(semaphore->semaphore);
		free(semaphore->semaphore);
	}
	return r;
#endif
}

#endif /* !_platform_sync_h_ */
