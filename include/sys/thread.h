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

#else
#include <pthread.h>
#include <sched.h>

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

static inline int thread_create2(pthread_t* thread, unsigned int stacksize, thread_proc func, void* param)
{
#if defined(OS_WINDOWS)
	// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682453.aspx
	// CreateThread function: By default, every thread has one megabyte of stack space. 

	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms682453%28v=vs.85%29.aspx
	// A thread in an executable that calls the C run-time library (CRT) 
	// should use the _beginthreadex and _endthreadex functions for thread management 
	// rather than CreateThread and ExitThread;

	//thread->handle = CreateThread(NULL, stacksize, (LPTHREAD_START_ROUTINE)func, param, 0, &thread->id);
	typedef unsigned int(__stdcall *thread_routine)(void *);
	thread->handle = (HANDLE)_beginthreadex(NULL, stacksize, (thread_routine)func, param, 0, (unsigned int*)&thread->id);
	return NULL == thread->handle ? -1 : 0;
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
	return r;
#endif
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
