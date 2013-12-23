#ifndef _platform_system_h_
#define _platform_system_h_

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>

typedef HMODULE module_t;

#elif defined(_MAC_)
#include <sys/param.h>
#include <sys/sysctl.h>

#else
#include <sys/types.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <errno.h>

typedef void* module_t;
#endif

#include <sys/timeb.h>
#include <stdio.h>

#ifndef IN
#define IN 
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

inline void system_sleep(IN size_t millisecond);

// Get CPU count
inline size_t system_getcpucount(void);
inline long long system_getcyclecount(void);

///@return second.milliseconds
inline double system_time(void);
///@return milliseconds
inline size_t system_tickcount(void);

inline int system_version(int* major, int* minor);

//////////////////////////////////////////////////////////////////////////
///
/// dynamic module load/unload
///
//////////////////////////////////////////////////////////////////////////
inline module_t system_load(const char* module);
inline int system_unload(module_t module);
inline void* system_getproc(module_t module, const char* producer);

//////////////////////////////////////////////////////////////////////////
///
/// implement
///
//////////////////////////////////////////////////////////////////////////
inline void system_sleep(IN size_t milliseconds)
{
#if defined(OS_WINDOWS)
	Sleep(milliseconds);
#else
	usleep(milliseconds*1000);
#endif
}

inline size_t system_getcpucount(void)
{
#if defined(OS_WINDOWS)
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;

#elif defined(_FREEBSD_) || defined(OS_MAC) || defined(_NETBSD_) || defined(_OPENBSD_)
	// FreeBSD, MacOS X, NetBSD, OpenBSD, etc.:
	int mib[4];
	size_t num;
	size_t len;

	mib[0] = CTL_HW;
	mib[1] = HW_AVAILCPU; // alternatively, try HW_NCPU;

	len = sizeof(num);
	sysctl(mib, 2, &num, &len, NULL, 0);
	if(num < 1)
	{
		mib[1] = HW_NCPU;
		sysctl(mib, 2, &num, &len, NULL, 0);

		if(num < 1)
			num = 1;
	}
	return num;

#elif defined(_HPUX_)
	// HPUX:
	return mpctl(MPC_GETNUMSPUS, NULL, NULL);

#elif defined(_IRIX_)
	// IRIX:
	return sysconf(_SC_NPROC_ONLN);

#else
	// linux, Solaris, & AIX
	return sysconf(_SC_NPROCESSORS_ONLN);

	//"cat /proc/cpuinfo | grep processor | wc -l"
#endif
}

inline long long system_getcyclecount(void)
{
#if defined(OS_WINDOWS)
	LARGE_INTEGER count;
	LARGE_INTEGER frequency;
	if(QueryPerformanceCounter(&count))
	{
		if(QueryPerformanceFrequency(&frequency))
		{
			return count.QuadPart/frequency.QuadPart;
		}
		else
		{
			return count.QuadPart;
		}
	}
#else
	return 0;
#endif
}

inline double system_time(void)
{
	struct timeb t;
	ftime(&t);
	return t.time+t.millitm*0.001;
}

inline size_t system_tickcount(void)
{
#if defined(OS_WINDOWS)
	LARGE_INTEGER freq;
	LARGE_INTEGER count;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&count);
	return (size_t)(count.QuadPart * 1000 / freq.QuadPart);
#else
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (size_t)((size_t)tp.tv_sec*1000 + tp.tv_nsec/1000000);
#endif
}

inline int system_version(int* major, int* minor)
{
#if defined(OS_WINDOWS)
	/*
	Operating system		Version number 
	Windows 8				6.2
	Windows Server 2012		6.2
	Windows 7				6.1
	Windows Server 2008 R2	6.1
	Windows Server 2008		6.0 
	Windows Vista			6.0 
	Windows Server 2003 R2	5.2 
	Windows Server 2003		5.2 
	Windows XP				5.1 
	Windows 2000			5.0 
	Windows Me				4.90 
	Windows 98				4.10 
	Windows NT 4.0			4.0 
	Windows 95				4.0 
	*/
	OSVERSIONINFO version;
	memset(&version, 0, sizeof(version));
	version.dwOSVersionInfoSize = sizeof(version);
	GetVersionEx(&version);
	*major = (int)(version.dwMajorVersion);
	*minor = (int)(version.dwMinorVersion);
	return 0;
#else
	struct utsname ver;
	if(0 != uname(&ver))
		return errno;
	if(2!=sscanf(ver.release, "%d.%d", major, minor))
		return -1;
	return 0;
#endif
}

inline module_t system_load(const char* module)
{
#if defined(OS_WINDOWS)
	return LoadLibraryExA(module, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
	return dlopen(module, RTLD_LAZY|RTLD_LOCAL);
#endif
}

inline int system_unload(module_t module)
{
#if defined(OS_WINDOWS)
	return FreeLibrary(module);
#else
	return dlclose(module);
#endif
}

inline void* system_getproc(module_t module, const char* producer)
{
#if defined(OS_WINDOWS)
	return GetProcAddress(module, producer);
#else
	return dlsym(module, producer);
#endif
}

#endif /* !_platform_system_h_ */
