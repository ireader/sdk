#ifndef _platform_process_h_
#define _platform_process_h_

#include <string.h>

#if defined(OS_WINDOWS)
#include <Windows.h>
#include <Psapi.h>

#if defined(_MSC_VER)
#pragma comment(lib, "Psapi.lib")
#endif

typedef DWORD pid_t;

#else
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>

extern char** environ;
#endif

//-------------------------------------------------------------------------------------
// int process_createve(const char* filename, process_create_param_t *param, pid_t* pid);
// int process_create(const char* filename, pid_t* pid);
// int process_kill(pid_t pid);
// int process_name(pid_t pid, char* name, size_t size);
// int process_selfname(char* name, size_t size);
// pid_t process_self(void);
//-------------------------------------------------------------------------------------

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


//////////////////////////////////////////////////////////////////////////
///
/// process
///
//////////////////////////////////////////////////////////////////////////
static inline int process_createve(const char* filename, process_create_param_t *param, pid_t* pid)
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

static inline int process_create(const char* filename, pid_t* pid)
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

static inline int process_kill(pid_t pid)
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

static inline pid_t process_self(void)
{
#if defined(OS_WINDOWS)
	return GetCurrentProcessId();
#else
	return getpid();
#endif
}

static inline int process_selfname(char* name, size_t size)
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

static inline int process_name(pid_t pid, char* name, size_t size)
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
	snprintf(filename, sizeof(filename), "/proc/%d/exe", (int)pid);
	len = readlink(filename, name, size-1);
	if(len <= 0)
		return (int)errno;
	name[len] = 0;
	return 0;
#endif
}

#endif /* !_platform_process_h_ */
