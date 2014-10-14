#ifndef _platform_semaphore_h_
#define _platform_semaphore_h_

#include <errno.h>

#if defined(OS_WINDOWS)
#include <Windows.h>
typedef HANDLE semaphore_t;

#else
#include <time.h>
#include <fcntl.h>
#include <semaphore.h>

typedef struct
{
	sem_t*	semaphore;
	char	name[256];
} semaphore_t;

#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT		ETIMEDOUT
#endif

#endif

/// semaphore:
/// Named semaphores(process-shared semaphore)/Unnamed semaphores(thread-shared semaphore)
///	multi-processor: yes
//-------------------------------------------------------------------------------------
// int semaphore_create(semaphore_t* semaphore, const char* name, long value);
// int semaphore_open(semaphore_t* semaphore, const char* name);
// int semaphore_wait(semaphore_t* semaphore);
// int semaphore_timewait(semaphore_t* semaphore, int timeout);
// int semaphore_trywait(semaphore_t* semaphore);
// int semaphore_post(semaphore_t* semaphore);
// int semaphore_destroy(semaphore_t* semaphore);
//-------------------------------------------------------------------------------------

// Windows: the name can contain any character except the backslash
// 0-success, other-error
inline int semaphore_create(semaphore_t* semaphore, const char* name, long value)
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
#ifndef OS_MAC
	else
	{
		// Unnamed semaphores (memory-based semaphores, thread-shared semaphore)
		semaphore->semaphore = (sem_t*)malloc(sizeof(sem_t));
		if(!semaphore->semaphore)
			return ENOMEM;
		return 0==sem_init(semaphore->semaphore, 0, value) ? 0 : errno;
	}
#endif
	// __APPLE__ / __MACH__
	// https://github.com/joyent/libuv/blob/master/src/unix/thread.c
	// semaphore_create/semaphore_destroy/semaphore_signal/semaphore_wait/semaphore_timedwait
	return -1;
#endif
}

// 0-success, other-error
inline int semaphore_open(semaphore_t* semaphore, const char* name)
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
inline int semaphore_wait(semaphore_t* semaphore)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*semaphore, INFINITE, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#else
	return sem_wait(semaphore->semaphore);
#endif
}

#if defined(OS_WINDOWS)
// 0-success, WAIT_TIMEOUT-timeout, other-error
inline int semaphore_timewait(semaphore_t* semaphore, int timeout)
{
	DWORD r = WaitForSingleObjectEx(*semaphore, timeout, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
}
#elif defined(OS_LINUX)
inline int semaphore_timewait(semaphore_t* semaphore, int timeout)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += timeout/1000;
	ts.tv_nsec += (timeout%1000)*1000000;
	return -1==sem_timedwait(semaphore->semaphore, &ts) ? errno : 0;
}
#endif

// 0-success, other-error
inline int semaphore_trywait(semaphore_t* semaphore)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*semaphore, 0, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#else
	return sem_trywait(semaphore->semaphore);
#endif
}

// 0-success, other-error
inline int semaphore_post(semaphore_t* semaphore)
{
#if defined(OS_WINDOWS)
	long r;
	return ReleaseSemaphore(*semaphore, 1, &r)?0:GetLastError();
#else
	return sem_post(semaphore->semaphore);
#endif
}

// 0-success, other-error
inline int semaphore_destroy(semaphore_t* semaphore)
{
#if defined(OS_WINDOWS)
	return CloseHandle(*semaphore)?0:GetLastError();
#else
	int r = -1;
	if(semaphore->name[0])
	{
		// sem_open
		r = sem_close(semaphore->semaphore);
		if(0 == r)
			r = sem_unlink(semaphore->name);
	}
#ifndef OS_MAC
	else
	{
		// sem_init
		r = sem_destroy(semaphore->semaphore);
		free(semaphore->semaphore);
	}
#endif
	return r;
#endif
}

#endif /* !_platform_semaphore_h_ */
