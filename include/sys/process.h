#ifndef _platform_process_h_
#define _platform_process_h_

#if defined(OS_WINDOWS)
#include <Windows.h>
#include <Psapi.h>
#include <process.h>

#if defined(_MSC_VER)
#pragma comment(lib, "Psapi.lib")
#endif

#ifndef STDCALL
#define STDCALL __stdcall
#endif

typedef unsigned int (__stdcall *thread_routine)(void *);

typedef struct
{
	DWORD id;
	HANDLE handle;
} thread_t;

typedef DWORD			process_t;

#else
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>

typedef pthread_t		thread_t;
typedef pid_t			process_t;

#ifndef STDCALL
#define STDCALL
#endif

extern char** environ;
#endif

#include <memory.h>
#include <errno.h>

#ifndef IN
#define IN 
#endif

#ifndef OUT
#define OUT
#endif

//////////////////////////////////////////////////////////////////////////
///
/// thread: Windows CreateThread/Linux pthread
///
//////////////////////////////////////////////////////////////////////////
typedef int (STDCALL *thread_proc)(IN void* param);

inline int thread_create(OUT thread_t* thread, IN thread_proc func, IN void* param);
inline int thread_destroy(IN thread_t thread);

// priority: [-15, 15]
// 0: normal / -15: idle / 15: critical
inline int thread_getpriority(IN thread_t thread, OUT int* priority);
inline int thread_setpriority(IN thread_t thread, IN int priority);

inline int thread_self(void);
inline int thread_getid(IN thread_t thread, OUT int* id);
inline int thread_isself(IN thread_t thread);

inline int process_create(IN const char* filename, OUT process_t* pid);
inline int process_kill(IN process_t pid);

inline process_t process_self(void);
inline int process_selfname(char* name, size_t size);
inline int process_name(process_t pid, char* name, size_t size);

typedef struct
{
	int bInheritHandles; // 1-inherit handles

#if defined(OS_WINDOWS)
	LPSTR lpCommandLine; // msdn: CreateProcessW, can modify the contents of this string
	LPSECURITY_ATTRIBUTES lpProcessAttributes;
	LPSECURITY_ATTRIBUTES lpThreadAttributes;
	DWORD dwCreationFlags;
	LPVOID lpEnvironment; // terminated by two zero bytes
	LPCSTR lpCurrentDirectory;
	STARTUPINFOA startupInfo;
#else
	char* const *argv;
	char* const *envp;
#endif
} process_create_param_t;

inline int process_createve(IN const char* filename, IN process_create_param_t *param, OUT process_t* pid);

//////////////////////////////////////////////////////////////////////////
///
/// thread: Windows CreateThread/Linux pthread
///
//////////////////////////////////////////////////////////////////////////
inline int thread_create(OUT thread_t* thread, IN thread_proc func, IN void* param)
{
#if defined(OS_WINDOWS)
	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms682453%28v=vs.85%29.aspx
	// A thread in an executable that calls the C run-time library (CRT) 
	// should use the _beginthreadex and _endthreadex functions for thread management 
	// rather than CreateThread and ExitThread;

	//thread->handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, param, 0, &thread->id);
	thread->handle = (HANDLE)_beginthreadex(NULL, 0, (thread_routine)func, param, 0, (unsigned int*)&thread->id);
	if(NULL == thread->handle)
		return -1;
	return 0;
#else
	typedef void* (*linux_thread_routine)(void*);
	return pthread_create(thread, NULL, (linux_thread_routine)func, param);
#endif
}

inline int thread_destroy(IN thread_t thread)
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

// priority: [-15, 15]
// 0: normal / -15: idle / 15: critical
inline int thread_getpriority(IN thread_t thread, OUT int* priority)
{
#if defined(OS_WINDOWS)
	int r = GetThreadPriority(thread.handle);
	if(THREAD_PRIORITY_ERROR_RETURN == r)
		return (int)GetLastError();

	*priority = r;
	return 0;
#else
	struct sched_param sched;
	int r = pthread_getschedparam(thread, priority, &sched);
	if(0 == r)
		*priority = sched.sched_priority;
	return r;
#endif
}

inline int thread_setpriority(IN thread_t thread, IN int priority)
{
#if defined(OS_WINDOWS)
	BOOL r = SetThreadPriority(thread.handle, priority);
	return TRUE==r?1:0;
#else
	struct sched_param sched;
	sched.sched_priority = 0;
	int r = pthread_setschedparam(thread, priority, &sched);
	return r;
#endif
}

inline int thread_self(void)
{
#if defined(OS_WINDOWS)
	DWORD id = GetCurrentThreadId();
	return (int)id;
#else
	return (int)pthread_self();
#endif
}

inline int thread_getid(IN thread_t thread, OUT int* id)
{
#if defined(OS_WINDOWS)
	*id = thread.id;
	//DWORD tid = GetThreadId(thread.handle); // >= vista
	//if(0 == tid)
	//	return GetLastError();
	//*id = (int)tid;
	return 0;
#else
	*id = (int)thread;
	return 0;
#endif
}

inline int thread_isself(IN thread_t thread)
{
#if defined(OS_WINDOWS)
	return thread.id==GetCurrentThreadId() ? 1 : 0;
#else
	return pthread_equal(pthread_self(),thread);
#endif
}

inline int thread_valid(IN thread_t thread)
{
#if defined(OS_WINDOWS)
	return 0 != thread.id ? 1 : 0;
#else
	return 0 != thread ? 1 : 0;
#endif
}


//////////////////////////////////////////////////////////////////////////
///
/// process
///
//////////////////////////////////////////////////////////////////////////
inline int process_createve(IN const char* filename, IN process_create_param_t *param, OUT process_t* pid)
{
#if defined(OS_WINDOWS)
	PROCESS_INFORMATION pi;
	memset(&pi, 0, sizeof(pi));
	param->startupInfo.cb = sizeof(param->startupInfo);

	if(!CreateProcessA(filename, 
					param->lpCommandLine, 
					param->lpProcessAttributes, 
					param->lpThreadAttributes,
					1==param->bInheritHandles?TRUE:FALSE, 
					param->dwCreationFlags,
					param->lpEnvironment, 
					param->lpCurrentDirectory, 
					&param->startupInfo, 
					&pi))
		return (int)GetLastError();

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	*pid = pi.dwProcessId;
	return 0;

#else
	int i;
	pid_t r = fork();
	if(r < 0)
	{
		return (int)errno;
	}
	else if(0 == r)
	{
		// man 2 execve:
		// By default, file descriptors remain open across an  execve().
		// 0-2 stdin/stdout/stderr
		if(1 != param->bInheritHandles)
		{
			for(i=3;i<getdtablesize();i++)
				close(i);
		}

		// child process
		r = execve(filename, param->argv, param->envp?param->envp:environ);
		if(r < 0)
			printf("process_create %s error: %d\n", filename, errno);
		return 0;
	}
	*pid = r;
	return 0;
#endif
}

inline int process_create(IN const char* filename, OUT process_t* pid)
{
    process_create_param_t param;
#if defined(OS_WINDOWS)
    memset(&param, 0, sizeof(param));
    param.startupInfo.cb = sizeof(param.startupInfo);
#else
    char* const argv[2] = { (char*)filename, NULL };
    memset(&param, 0, sizeof(param));
    param.argv = argv;
#endif

    return process_createve(filename, &param, pid);
}

inline int process_kill(IN process_t pid)
{
#if defined(OS_WINDOWS)
	HANDLE handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
	if(!handle)
		return (int)GetLastError();

	if(!TerminateProcess(handle, 0))
	{
		CloseHandle(handle);
		return (int)GetLastError();
	}

	CloseHandle(handle);
	return 0;
#else
	if(0 != kill(pid, SIGKILL))
		return -(int)errno;
	return 0;
#endif
}

inline process_t process_self(void)
{
#if defined(OS_WINDOWS)
	return GetCurrentProcessId();
#else
	return getpid();
#endif
}

inline int process_selfname(OUT char* name, IN size_t size)
{
#if defined(OS_WINDOWS)
	if(0 == GetModuleFileNameA(NULL, name, size))
		return (int)GetLastError();
	return 0;
#else
	ssize_t len = readlink("/proc/self/exe", name, size-1);
	if(len <= 0)
		return (int)errno;
	name[len] = 0;
	return 0;
#endif
}

inline int process_name(IN process_t pid, OUT char* name, IN size_t size)
{
#if defined(OS_WINDOWS)
	DWORD r = 0;
	HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pid);
	if(!h)
		return (int)GetLastError();

	r = GetModuleFileNameExA(h, NULL, name, size);
	CloseHandle(h);
	return 0==r ? (int)GetLastError() : 0;
#else
	ssize_t len;
	char filename[64] = {0};
	sprintf(filename, "/proc/%d/exe", (int)pid);
	len = readlink(filename, name, size-1);
	if(len <= 0)
		return (int)errno;
	name[len] = 0;
	return 0;
#endif
}

#endif /* !_platform_process_h_ */
