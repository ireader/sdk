#ifndef _platform_semaphore_h_
#define _platform_semaphore_h_

#include <errno.h>

#if defined(OS_WINDOWS)
#include <Windows.h>
typedef HANDLE sema_t;

#else
#include <time.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(OS_MAC)
#include <sys/time.h> // gettimeofday
#include <dispatch/dispatch.h>
#include <dispatch/time.h>
#endif

typedef struct
{
	sem_t*	semaphore;
	char	name[256];
#if defined(OS_MAC)
    dispatch_semaphore_t dispatch;
#endif
} sema_t;

#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT		ETIMEDOUT
#endif

#endif

/// semaphore:
/// Named semaphores(process-shared semaphore)/Unnamed semaphores(thread-shared semaphore)
///	multi-processor: yes
//-------------------------------------------------------------------------------------
// int sema_create(semaphore_t* semaphore, const char* name, long value);
// int sema_open(semaphore_t* semaphore, const char* name);
// int sema_wait(semaphore_t* semaphore);
// int sema_timewait(semaphore_t* semaphore, int timeout);
// int sema_trywait(semaphore_t* semaphore);
// int sema_post(semaphore_t* semaphore);
// int sema_destroy(semaphore_t* semaphore);
//-------------------------------------------------------------------------------------

// Windows: the name can contain any character except the backslash
// 0-success, other-error
#if defined(OS_WINDOWS)
static inline int sema_create(sema_t* sema, const char* name, long value)
{
	HANDLE handle = CreateSemaphoreA(NULL, value, 0x7FFFFFFF, name);
	if(NULL == handle)
		return GetLastError();
	*sema = handle;
	return 0;
}
#else
static inline int sema_create(sema_t* sema, const char* name, long value)
{
	memset(sema->name, 0, sizeof(sema->name));
	if(name && *name)
	{
		// POSIX Named semaphores(process-shared semaphore)
        sema->semaphore = sem_open(name, O_CREAT|O_EXCL|O_RDWR, 0777, value);
		if(SEM_FAILED == sema->semaphore)
			return errno;
		strncpy(sema->name, name, sizeof(sema->name)-1);
		return 0;
	}
    else
    {
#if defined(OS_MAC)
        // __APPLE__ / __MACH__
        // https://github.com/joyent/libuv/blob/master/src/unix/thread.c
        // semaphore_create/semaphore_destroy/semaphore_signal/semaphore_wait/semaphore_timedwait
        sema->dispatch = dispatch_semaphore_create(value);
        return sema->dispatch ? 0 : -1;
#else
		// Unnamed semaphores (memory-based semaphores, thread-shared semaphore)
        sema->semaphore = (sem_t*)malloc(sizeof(sem_t));
		if(!sema->semaphore)
			return ENOMEM;
		return 0==sem_init(sema->semaphore, 0, value) ? 0 : errno;
#endif
	}
}
#endif

// 0-success, other-error
static inline int sema_open(sema_t* sema, const char* name)
{
	if(!name || !*name)
		return EINVAL;

#if defined(OS_WINDOWS)
	*sema = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, name);
	return *sema ? 0 : GetLastError();
#else
    sema->semaphore = sem_open(name, 0);
	if(SEM_FAILED == sema->semaphore)
		return errno;

	memset(sema->name, 0, sizeof(sema->name));
	return 0;
#endif
}

// 0-success, other-error
static inline int sema_wait(sema_t* sema)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*sema, INFINITE, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#else
    int r;
#if defined(OS_MAC)
    // OSX unnamed semaphore
    if(!sema->name[0])
        return 0 == dispatch_semaphore_wait(sema->dispatch, DISPATCH_TIME_FOREVER) ? 0 : WAIT_TIMEOUT;
#endif
    
    // http://man7.org/linux/man-pages/man7/signal.7.html
    r = sem_wait(sema->semaphore);
    while(-1 == r && EINTR == errno)
        r = sem_wait(sema->semaphore);
    return r;
#endif
}

#if defined(OS_WINDOWS)
// 0-success, WAIT_TIMEOUT-timeout, other-error
static inline int sema_timewait(sema_t* sema, int timeout)
{
	DWORD r = WaitForSingleObjectEx(*sema, timeout, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
}
#elif defined(OS_MAC)
static inline int sema_timewait(sema_t* sema, int timeout)
{
    if(!sema->name[0])
        return 0==dispatch_semaphore_wait(sema->dispatch, dispatch_time(DISPATCH_TIME_NOW, timeout)) ? 0 : WAIT_TIMEOUT;
    
    // OSX don't have sem_timedwait
    return -1;
}
#else
static inline int sema_timewait(sema_t* sema, int timeout)
{
    int r;
    struct timespec ts;
    
#if defined(CLOCK_REALTIME)
    clock_gettime(CLOCK_REALTIME, &ts);
#else
    // POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
#endif

	ts.tv_sec += timeout/1000;
	ts.tv_nsec += (timeout%1000)*1000000;

	// tv_nsec >= 1000000000 ==> EINVAL
	ts.tv_sec += ts.tv_nsec / 1000000000;
	ts.tv_nsec %= 1000000000;
    
    // http://man7.org/linux/man-pages/man7/signal.7.html
    r = sem_timedwait(sema->semaphore, &ts);
    while(-1 == r && EINTR == errno)
        r = sem_timedwait(sema->semaphore, &ts);
	return -1 == r ? errno : 0;
}
#endif

// 0-success, other-error
static inline int sema_trywait(sema_t* sema)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*sema, 0, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#else
    
#if defined(OS_MAC)
    if(!sema->name[0])
        return 0==dispatch_semaphore_wait(sema->dispatch, DISPATCH_TIME_NOW) ? 0 : WAIT_TIMEOUT;
#endif
    
	return sem_trywait(sema->semaphore);
#endif
}

// 0-success, other-error
static inline int sema_post(sema_t* sema)
{
#if defined(OS_WINDOWS)
	long r;
	return ReleaseSemaphore(*sema, 1, &r)?0:GetLastError();
#else
    
#if defined(OS_MAC)
    if(!sema->name[0])
    {
        dispatch_semaphore_signal(sema->dispatch);
        return 0;
    }
#endif
    
	return sem_post(sema->semaphore);
#endif
}

// 0-success, other-error
static inline int sema_destroy(sema_t* sema)
{
#if defined(OS_WINDOWS)
	return CloseHandle(*sema)?0:GetLastError();
#else
	int r = -1;
	if(sema->name[0])
	{
		// sem_open
		r = sem_close(sema->semaphore);
		if(0 == r)
			r = sem_unlink(sema->name);
	}
	else
	{
#if defined(OS_MAC)
        r = 0;
        free(sema->dispatch);
#else
		// sem_init
		r = sem_destroy(sema->semaphore);
		free(sema->semaphore);
#endif
	}
    
    return r;
#endif
}

#endif /* !_platform_semaphore_h_ */
