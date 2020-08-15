#ifndef _platform_system_h_
#define _platform_system_h_

#if defined(OS_WINDOWS)
#include <Windows.h>

typedef HMODULE module_t;
typedef DWORD   useconds_t;
typedef FARPROC funcptr_t;

#else
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <errno.h>
#include <time.h>

typedef void* module_t;
typedef void (*funcptr_t)(void);
#endif

#if defined(OS_MAC)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <mach/mach_time.h>  
#endif

#include <stdint.h>
#include <stdio.h>

//-----------------------------------------------------------------------
// void system_sleep(useconds_t millisecond);
// uint64_t system_time(void);
// uint64_t system_clock(void);
// int64_t system_getcyclecount(void);
// size_t system_getcpucount(void);
//
// int system_version(int* major, int* minor);
// module_t system_load(const char* module);
// int system_unload(module_t module);
// funcptr_t system_getproc(module_t module, const char* producer);
//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
///
/// implement
///
//////////////////////////////////////////////////////////////////////////
static inline void system_sleep(useconds_t milliseconds)
{
#if defined(OS_WINDOWS)
	Sleep(milliseconds);
#else
	usleep(milliseconds*1000);
#endif
}

static inline size_t system_getcpucount(void)
{
#if defined(OS_WINDOWS)
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;

#elif defined(OS_MAC) || defined(_FREEBSD_) || defined(_NETBSD_) || defined(_OPENBSD_)
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

static inline int64_t system_getcyclecount(void)
{
#if defined(OS_WINDOWS)
	LARGE_INTEGER freq;
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	QueryPerformanceFrequency(&freq);
#else
#endif
	return 0;
}

/// milliseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
static inline uint64_t system_time(void)
{
#if defined(OS_WINDOWS)
	uint64_t t;
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	t = (uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
	return t / 10000 - 11644473600000ULL; /* Jan 1, 1601 */
#elif defined(OS_MAC)
	uint64_t tick;
	mach_timebase_info_data_t timebase;
	tick = mach_absolute_time();
	mach_timebase_info(&timebase);
	return tick * timebase.numer / timebase.denom / 1000000;
#else
#if defined(CLOCK_REALTIME)
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return (uint64_t)tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
#else
	// POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
#endif
}

///@return milliseconds(relative time)
static inline uint64_t system_clock(void)
{
#if defined(OS_WINDOWS)
	LARGE_INTEGER freq;
	LARGE_INTEGER count;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&count);
	return (uint64_t)count.QuadPart * 1000 / freq.QuadPart;
#elif defined(OS_MAC)
	uint64_t tick;
	mach_timebase_info_data_t timebase;
	tick = mach_absolute_time();
	mach_timebase_info(&timebase);
	return tick * timebase.numer / timebase.denom / 1000000;
#else
#if defined(CLOCK_MONOTONIC)
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (uint64_t)tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
#else
	// POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
#endif
}

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996) // GetVersionEx
#pragma warning(disable: 28159)
#endif
static inline int system_version(int* major, int* minor)
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
	if(2!=sscanf(ver.release, "%8d.%8d", major, minor))
		return -1;
	return 0;
#endif
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif


//////////////////////////////////////////////////////////////////////////
///
/// dynamic module load/unload
///
//////////////////////////////////////////////////////////////////////////
static inline module_t system_load(const char* module)
{
#if defined(OS_WINDOWS)
	return LoadLibraryExA(module, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
	return dlopen(module, RTLD_LAZY|RTLD_LOCAL);
#endif
}

static inline int system_unload(module_t module)
{
#if defined(OS_WINDOWS)
	return FreeLibrary(module);
#else
	return dlclose(module);
#endif
}

static inline funcptr_t system_getproc(module_t module, const char* producer)
{
#if defined(OS_WINDOWS)
	return GetProcAddress(module, producer);
#else
	// https://linux.die.net/man/3/dlsym
	// cosine = (double (*)(double)) dlsym(handle, "cos")
	// ===> *(void **) (&cosine) = dlsym(handle, "cos");
	return (funcptr_t)dlsym(module, producer);
#endif
}

#endif /* !_platform_system_h_ */
