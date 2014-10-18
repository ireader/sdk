#ifndef _platform_event_h_
#define _platform_event_h_

#if defined(OS_WINDOWS)
#include <Windows.h>
typedef HANDLE	event_t;

#else
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
typedef struct
{
	int count; // fixed pthread_cond_signal/pthread_cond_wait call order
	pthread_cond_t event;
	pthread_mutex_t mutex;
} event_t;

#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT		ETIMEDOUT
#endif

#endif

/// event: Windows Event/Linux condition variable
///	multi-processor: no
//-------------------------------------------------------------------------------------
// int event_create(event_t* event);
// int event_destroy(event_t* event);
// int event_wait(event_t* event);
// int event_timewait(event_t* event, int timeout);
// int event_signal(event_t* event);
// int event_reset(event_t* event);
//-------------------------------------------------------------------------------------

inline int event_create(event_t* event)
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

inline int event_destroy(event_t* event)
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
inline int event_wait(event_t* event)
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
inline int event_timewait(event_t* event, int timeout)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*event, timeout, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#else

#if defined(OS_LINUX)
	int r = 0;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_sec += timeout/1000;
	ts.tv_nsec += (timeout%1000)*1000000;
#else
	int r = 0;
	struct timeval tv;
	struct timespec ts;
	gettimeofday(&tv, NULL);
	ts.tv_sec = tv.tv_sec + timeout/1000;
	ts.tv_nsec = tv.tv_usec * 1000 + (timeout%1000)*1000000;
#endif

	pthread_mutex_lock(&event->mutex);
	if(0 == event->count)
		r = pthread_cond_timedwait(&event->event, &event->mutex, &ts);
	event->count = 0;
	pthread_mutex_unlock(&event->mutex);
	return r;
#endif
}

inline int event_signal(event_t* event)
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

inline int event_reset(event_t* event)
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

#endif /* !_platform_event_h_ */
