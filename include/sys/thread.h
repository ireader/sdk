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
#elif defined(OS_RTOS)
#include <string.h>
#ifndef STDCALL
#define STDCALL
#endif
#define RTOS_THREAD_DEFAULT_STACK_SIZE (2048)

#if defined(OS_RTTHREAD)
#include "rtthread.h"
#define pthread_t rt_thread_t
typedef pthread_t tid_t;

enum thread_priority
{
	THREAD_PRIORITY_IDLE			= RT_THREAD_PRIORITY_MAX - 1,
	THREAD_PRIORITY_LOWEST			= 5 * RT_THREAD_PRIORITY_MAX / 6 - 1,
	THREAD_PRIORITY_BELOW_NORMAL	= 4 * RT_THREAD_PRIORITY_MAX / 6 - 1,
	THREAD_PRIORITY_NORMAL			= 3 * RT_THREAD_PRIORITY_MAX / 6 - 1,
	THREAD_PRIORITY_ABOVE_NORMAL	= 2 * RT_THREAD_PRIORITY_MAX / 6 - 1,
	THREAD_PRIORITY_HIGHEST			= RT_THREAD_PRIORITY_MAX / 6 - 1,
	THREAD_PRIORITY_TIME_CRITICAL	= 0,
};
#elif defined(OS_FREERTOS)
// enable configUSE_APPLICATION_TASK_TAG in FreeRTOSConfig.h
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define pthread_t uint32_t
typedef pthread_t tid_t;

enum thread_priority
{
	THREAD_PRIORITY_IDLE			= 0,
	THREAD_PRIORITY_LOWEST			= configMAX_PRIORITIES / 6 - 1,
	THREAD_PRIORITY_BELOW_NORMAL	= 2 * configMAX_PRIORITIES / 6 - 1,
	THREAD_PRIORITY_NORMAL			= 3 * configMAX_PRIORITIES / 6 - 1,
	THREAD_PRIORITY_ABOVE_NORMAL	= 4 * configMAX_PRIORITIES / 6 - 1,
	THREAD_PRIORITY_HIGHEST			= 5 * configMAX_PRIORITIES / 6 - 1,
	THREAD_PRIORITY_TIME_CRITICAL	= configMAX_PRIORITIES - 1,
};
#else
	#error "This rtos is not supported"
#endif

#else
#include <pthread.h>
#include <sched.h>
#include <sys/prctl.h>

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
// int thread_create3(pthread_t* thread, const char *name, unsigned int stacksize, thread_proc func, void* param);
// int thread_create2(pthread_t* thread, unsigned int stacksize, thread_proc func, void* param);
// int thread_create(pthread_t* thread, thread_proc func, void* param);
// int thread_destroy(pthread_t thread);
// int thread_detach(pthread_t thread);
// int thread_getpriority(pthread_t thread, int* priority);
// int thread_setpriority(pthread_t thread, int priority);
// int thread_isself(pthread_t thread);
// int thread_valid(pthread_t thread);
// int thread_yield(void);
// tid_t thread_getid(pthread_t thread);
// pthread_t thread_self(void);
//-------------------------------------------------------------------------------------

typedef int (STDCALL *thread_proc)(void* param);

#if defined(OS_RTOS)
typedef struct {
	thread_proc func;
	void *arg;
	volatile pthread_t join_task;
	volatile uint8_t detached;
	volatile uint8_t exited;
#if defined(OS_RTTHREAD)
	rt_event_t event;
#endif
} *thread_rtos_arg_t;

static void thread_rtos_entry(void *arg) {
	thread_rtos_arg_t thread_rtos_arg = (thread_rtos_arg_t)arg;

	thread_rtos_arg->func(thread_rtos_arg->arg);

	if(thread_rtos_arg->detached)
	{
		free(thread_rtos_arg);
#if defined(OS_RTTHREAD)
		rt_thread_delete(rt_thread_self());
#elif defined(OS_FREERTOS)
		vTaskDelete(NULL);
#endif
	}
	else
	{
#if defined(OS_RTTHREAD)
		if(thread_rtos_arg->join_task)
			rt_event_send(thread_rtos_arg->event, 1);
		else
			thread_rtos_arg->exited = 1;
		rt_thread_suspend(rt_thread_self());
#elif defined(OS_FREERTOS)
		if(thread_rtos_arg->join_task)
			xTaskNotify((TaskHandle_t)thread_rtos_arg->join_task, 0, eNoAction);
		else
			thread_rtos_arg->exited = 1;
		vTaskSuspend(NULL);
#endif
	}
}
#endif

static inline int thread_create3(pthread_t* thread, const char *name, unsigned int stacksize, thread_proc func, void* param)
{
#if defined(OS_WINDOWS)
	// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682453.aspx
	// CreateThread function: By default, every thread has one megabyte of stack space. 

	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms682453%28v=vs.85%29.aspx
	// A thread in an executable that calls the C run-time library (CRT) 
	// should use the _beginthreadex and _endthreadex functions for thread management 
	// rather than CreateThread and ExitThread;

	//thread->handle = CreateThread(NULL, stacksize, (LPTHREAD_START_ROUTINE)func, param, 0, &thread->id);
	// TODO: Windows set thread name
	(void)name;
	typedef unsigned int(__stdcall *thread_routine)(void *);
	thread->handle = (HANDLE)_beginthreadex(NULL, stacksize, (thread_routine)func, param, 0, (unsigned int*)&thread->id);
	return NULL == thread->handle ? -1 : 0;
#elif defined(OS_RTOS)

	stacksize = stacksize ? stacksize : RTOS_THREAD_DEFAULT_STACK_SIZE;
	thread_rtos_arg_t thread_rtos_arg = (thread_rtos_arg_t)malloc(sizeof(*thread_rtos_arg));
	memset(thread_rtos_arg, 0, sizeof(*thread_rtos_arg));
	if (NULL == thread_rtos_arg) return -1;

	thread_rtos_arg->func = func;
	thread_rtos_arg->arg = param;
#if defined(OS_RTTHREAD)
	thread_rtos_arg->event = rt_event_create("thread_event", RT_IPC_FLAG_FIFO);
	if(NULL == thread_rtos_arg->event) {
		free(thread_rtos_arg);
		return -1;
	}
	*thread = rt_thread_create(name, thread_rtos_entry, thread_rtos_arg, stacksize, THREAD_PRIORITY_NORMAL, 10);
	if(NULL == *thread) {
		free(thread_rtos_arg);
		return -1;
	}
	(*thread)->user_data = (rt_uint32_t)thread_rtos_arg;
	rt_thread_startup(*thread);
	return 0;
#elif defined(OS_FREERTOS)
	BaseType_t ret = xTaskCreate(thread_rtos_entry, name, stacksize, thread_rtos_arg, THREAD_PRIORITY_NORMAL, (TaskHandle_t*)thread);
	if(pdTRUE != ret) {
		free(thread_rtos_arg);
		return -1;
	}
	vTaskSetApplicationTaskTag(*((TaskHandle_t*)thread), (TaskHookFunction_t)thread_rtos_arg);
	return 0;
#endif

#else
	// https://linux.die.net/man/3/pthread_create
	// On Linux/x86-32, the default stack size for a new thread is 2 megabytes(10M 64bits)

	// http://udrepper.livejournal.com/20948.html
	// mallopt(M_ARENA_MAX, cpu); // limit multithread virtual memory
	typedef void* (*linux_thread_routine)(void*);
	int r;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, stacksize);
	r = pthread_create(thread, &attr, (linux_thread_routine)func, param);
	pthread_attr_destroy(&attr);
	if (0 == r) {
		return prctl(PR_SET_NAME, name);
	}
	return r;
#endif
}

static inline int thread_create2(pthread_t* thread, unsigned int stacksize, thread_proc func, void* param)
{
	return thread_create3(thread, "thread", stacksize, func, param);
}

static inline int thread_create(pthread_t* thread, thread_proc func, void* param)
{
	return thread_create2(thread, 0, func, param);
}

static inline int thread_destroy(pthread_t thread)
{
#if defined(OS_WINDOWS)
	if(thread.id != GetCurrentThreadId())
		WaitForSingleObjectEx(thread.handle, INFINITE, TRUE);
	CloseHandle(thread.handle);
	return 0;
#elif defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	rt_uint32_t evt;
	thread_rtos_arg_t thread_rtos_arg = (thread_rtos_arg_t)thread->user_data;
	if(NULL == thread_rtos_arg) return -1;

	if(thread == rt_thread_self())
	{
		thread_rtos_arg->detached = 1;
	}
	else
	{
		if(!thread_rtos_arg->exited)
		{
			thread_rtos_arg->join_task = rt_thread_self();
			rt_event_recv(thread_rtos_arg->event, 1, RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &evt);
			(void)evt;
		}
		rt_thread_delete(thread);
		free(thread_rtos_arg);
	}
#elif defined(OS_FREERTOS)
	thread_rtos_arg_t thread_rtos_arg = (thread_rtos_arg_t)xTaskGetApplicationTaskTag((TaskHandle_t)thread);
	if(NULL == thread_rtos_arg) return -1;

	if((TaskHandle_t)thread == xTaskGetCurrentTaskHandle())
	{
		thread_rtos_arg->detached = 1;
	}
	else
	{
		if(!thread_rtos_arg->exited)
		{
			thread_rtos_arg->join_task = (pthread_t)xTaskGetCurrentTaskHandle();
			xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
		}
		vTaskDelete((TaskHandle_t)thread);
		free(thread_rtos_arg);
	}
	return 0;
#endif

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
#elif defined(OS_RTOS)

	thread_rtos_arg_t thread_rtos_arg;
#if defined(OS_RTTHREAD)
	thread_rtos_arg = (thread_rtos_arg_t)thread->user_data;
#elif defined(OS_FREERTOS)
	thread_rtos_arg = (thread_rtos_arg_t)xTaskGetApplicationTaskTag((TaskHandle_t)thread);
#endif
	if(NULL == thread_rtos_arg) return -1;
	thread_rtos_arg->detached = 1;
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
#elif defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	return thread->current_priority;
#elif defined(OS_FREERTOS)
	return uxTaskPriorityGet((TaskHandle_t)thread);
#endif

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
#elif defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	return rt_thread_control(thread, RT_THREAD_CTRL_CHANGE_PRIORITY, &priority);
#elif defined(OS_FREERTOS)
	vTaskPrioritySet((TaskHandle_t)thread, (UBaseType_t)priority);
	return 0;
#endif

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

static inline pthread_t thread_self(void)
{
#if defined(OS_WINDOWS)
	pthread_t t;
	t.handle = GetCurrentThread();
	t.id = GetCurrentThreadId();
	return t;
#elif defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	return rt_thread_self();
#elif defined(OS_FREERTOS)
	return (pthread_t)xTaskGetCurrentTaskHandle();
#endif

#else
	return pthread_self();
#endif
}

static inline tid_t thread_getid(pthread_t thread)
{
#if defined(OS_WINDOWS)
	//return GetThreadId(thread.handle); // >= vista
	return thread.id;
#else
	return thread;
#endif
}

static inline int thread_isself(pthread_t thread)
{
#if defined(OS_WINDOWS)
	return thread.id==GetCurrentThreadId() ? 1 : 0;
#elif defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	return (thread == rt_thread_self());
#elif defined(OS_FREERTOS)
	return ((TaskHandle_t)thread == xTaskGetCurrentTaskHandle());
#endif

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
	return SwitchToThread() ? 0 : -1;
#elif defined(OS_RTOS)

#if defined(OS_RTTHREAD)
	return rt_thread_yield();
#elif defined(OS_FREERTOS)
	taskYIELD();
	return 0;
#endif

#else
	return sched_yield();
#endif
}

#if defined(OS_WINDOWS_XP)
typedef DWORD KPRIORITY;
typedef struct _CLIENT_ID
{
	PVOID UniqueProcess;
	PVOID UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

typedef struct _THREAD_BASIC_INFORMATION
{
	NTSTATUS                ExitStatus;
	PVOID                   TebBaseAddress;
	CLIENT_ID               ClientId;
	KAFFINITY               AffinityMask;
	KPRIORITY               Priority;
	KPRIORITY               BasePriority;
} THREAD_BASIC_INFORMATION, *PTHREAD_BASIC_INFORMATION;

typedef NTSTATUS(__stdcall *NtQueryInformationThread)(HANDLE ThreadHandle, int ThreadInformationClass, PVOID ThreadInformation, ULONG ThreadInformationLength, PULONG ReturnLength);

static inline tid_t thread_getid_xp(HANDLE handle)
{
	// NT_TIB* tib = (NT_TIB*)__readfsdword(0x18);
	HMODULE module;
	THREAD_BASIC_INFORMATION tbi;
	memset(&tbi, 0, sizeof(tbi));
	module = GetModuleHandleA("ntdll.dll");
	NtQueryInformationThread fp = (NtQueryInformationThread)GetProcAddress(module, "NtQueryInformationThread");
	fp(handle, 0/*ThreadBasicInformation*/, &tbi, sizeof(tbi), NULL);
	return (tid_t)tbi.ClientId.UniqueThread;
}
#endif

#endif /* !_platform_thread_h_ */
