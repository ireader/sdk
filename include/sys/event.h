#ifndef _platform_event_h_
#define _platform_event_h_

#if defined(OS_WINDOWS)
#include <Windows.h>
typedef HANDLE	event_t;
#elif defined(OS_RTOS)

#define OS_RTOS_EVENT 1
#if defined(OS_RTTHREAD)
#include <stddef.h>
#include "rtthread.h"
typedef struct rt_event	event_t;
#elif defined(OS_FREERTOS)
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
typedef EventGroupHandle_t	event_t;
#else
	#error "This rtos is not supported"
#endif

#else
#include <sys/time.h> // gettimeofday
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <time.h> // clock_gettime
typedef struct
{
	int count; // fixed pthread_cond_signal/pthread_cond_wait call order
	pthread_cond_t event;
	pthread_mutex_t mutex;
} event_t;

#endif

#ifndef WAIT_TIMEOUT
#ifndef ETIMEDOUT
#define ETIMEDOUT			110
#endif
#define WAIT_TIMEOUT		ETIMEDOUT
#endif

/// event: Windows Event/Linux condition variable
///	multi-processor: no
//-------------------------------------------------------------------------------------
// int event_create(event_t* event);
// int event_destroy(event_t* event);
// int event_wait(event_t* event);
// int event_timewait(event_t* event, int timeout);
// int event_signal(event_t* event);
// int event_signal_isr(event_t* event);
// int event_reset(event_t* event);
//-------------------------------------------------------------------------------------

static inline int event_create(event_t* event)
{
#if defined(OS_WINDOWS)
	HANDLE h = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(NULL==h)
		return (int)GetLastError();
	*event = h;
	return 0;
#elif defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	rt_err_t ret = rt_event_init(event, "event", RT_IPC_FLAG_FIFO);
	return (RT_EOK == ret) ? 0 : -1;
#elif defined(OS_FREERTOS)
	*event = xEventGroupCreate();
	return (NULL != *event) ? 0 : -1;
#endif

#else
	int r;
#if defined(OS_LINUX) && defined(CLOCK_MONOTONIC) && defined(__USE_XOPEN2K)
	pthread_condattr_t attr;
#endif
    pthread_mutexattr_t mutex;

	pthread_mutexattr_init(&mutex);
#if defined(OS_LINUX)
	pthread_mutexattr_settype(&mutex, PTHREAD_MUTEX_RECURSIVE_NP);
#else
	pthread_mutexattr_settype(&mutex, PTHREAD_MUTEX_RECURSIVE);
#endif
	pthread_mutex_init(&event->mutex, &mutex);

#if defined(OS_LINUX) && defined(CLOCK_MONOTONIC) && defined(__USE_XOPEN2K)
	pthread_condattr_init(&attr);
	pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	r = pthread_cond_init(&event->event, &attr);
	pthread_condattr_destroy(&attr);
#else
	r = pthread_cond_init(&event->event, NULL);
#endif

	event->count = 0;
	return r;
#endif
}

static inline int event_destroy(event_t* event)
{
#if defined(OS_WINDOWS)
	BOOL r = CloseHandle(*event);
	return r?0:(int)GetLastError();
#elif defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	rt_err_t ret = rt_event_detach(event);
	return (RT_EOK == ret) ? 0 : -1;
#elif defined(OS_FREERTOS)
	vEventGroupDelete(*event);
	*event = NULL;
	return 0;
#endif

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
static inline int event_wait(event_t* event)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*event, INFINITE, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#elif defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	rt_uint32_t e;
	rt_err_t ret = rt_event_recv(event, OS_RTOS_EVENT, OS_RTOS_EVENT_FLAG_OR, RT_WAITING_FOREVER, &e);
	return (RT_EOK == ret) ? 0 : -1;
#elif defined(OS_FREERTOS)
	xEventGroupWaitBits(*event, OS_RTOS_EVENT, pdTRUE, pdTRUE, portMAX_DELAY);
	return 0;
#endif

#else
	int r = 0;
	pthread_mutex_lock(&event->mutex);
	if(0 == event->count)
		r = pthread_cond_wait(&event->event, &event->mutex); // These functions shall not return an error code of [EINTR].
	event->count = 0;
	pthread_mutex_unlock(&event->mutex);
	return r;
#endif
}

// 0-success, WAIT_TIMEOUT-timeout, other-error
static inline int event_timewait(event_t* event, int timeout)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*event, timeout, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#elif defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	rt_uint32_t e;
	rt_err_t ret = rt_event_recv(event, OS_RTOS_EVENT, OS_RTOS_EVENT_FLAG_OR, timeout / (1000 / RT_TICK_PER_SECOND), &e);
	return (RT_EOK == ret) ? 0 : (-RT_ETIMEOUT == ret) ? WAIT_TIMEOUT : ret;
#elif defined(OS_FREERTOS)
	EventBits_t ret = xEventGroupWaitBits(*event, OS_RTOS_EVENT, pdTRUE, pdTRUE, pdMS_TO_TICKS(timeout));
	return (OS_RTOS_EVENT == ret) ? 0 : WAIT_TIMEOUT;
#endif

#else

#if defined(OS_LINUX) && defined(CLOCK_REALTIME)
	int r = 0;
	struct timespec ts;
#if defined(CLOCK_MONOTONIC) && defined(__USE_XOPEN2K)
	clock_gettime(CLOCK_MONOTONIC, &ts);
#else
	clock_gettime(CLOCK_REALTIME, &ts);
#endif
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

	// tv_nsec >= 1000000000 ==> EINVAL
	ts.tv_sec += ts.tv_nsec / 1000000000;
	ts.tv_nsec %= 1000000000;

	pthread_mutex_lock(&event->mutex);
	if(0 == event->count)
		r = pthread_cond_timedwait(&event->event, &event->mutex, &ts); // These functions shall not return an error code of [EINTR].
	event->count = 0;
	pthread_mutex_unlock(&event->mutex);
	return r;
#endif
}

static inline int event_signal(event_t* event)
{
#if defined(OS_WINDOWS)
	return SetEvent(*event)?0:(int)GetLastError();
#elif defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	rt_err_t ret = rt_event_send(event, OS_RTOS_EVENT);
	return (RT_EOK == ret) ? 0 : -1;
#elif defined(OS_FREERTOS)
	xEventGroupSetBits(*event, OS_RTOS_EVENT);
	return 0;
#endif

#else
	int r;
	pthread_mutex_lock(&event->mutex);
	event->count = 1;
	r = pthread_cond_signal(&event->event);
	pthread_mutex_unlock(&event->mutex);
	return r;
#endif
}

static inline int event_signal_isr(event_t* event)
{
#if defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	return event_signal(event);
#elif defined(OS_FREERTOS)
	xEventGroupSetBitsFromISR(*event, OS_RTOS_EVENT, NULL);
	return 0;
#endif

#else
	return -1;
#endif
}

static inline int event_reset(event_t* event)
{
#if defined(OS_WINDOWS)
	return ResetEvent(*event)?0:(int)GetLastError();
#elif defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	rt_err_t ret = rt_event_control(event, RT_IPC_CMD_RESET, NULL);
	return (RT_EOK == ret) ? 0 : -1;
#elif defined(OS_FREERTOS)
	xEventGroupWaitBits(*event, OS_RTOS_EVENT, pdTRUE, pdTRUE, 0);
	return 0;
#endif

#else
	pthread_mutex_lock(&event->mutex);
	event->count = 0;
	pthread_mutex_unlock(&event->mutex);
	return 0;
#endif
}

#endif /* !_platform_event_h_ */
