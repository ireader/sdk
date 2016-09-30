#ifndef _platform_onetime_h_
#define _platform_onetime_h_

// One-Time Initialization
// https://msdn.microsoft.com/en-us/library/aa363808(VS.85).aspx
// https://msdn.microsoft.com/en-us/library/ms686934(VS.85).aspx
// https://linux.die.net/man/3/pthread_once

#if defined(OS_WINDOWS)
#include <Windows.h>
typedef INIT_ONCE			onetime_t;
#define ONETIME_INIT		INIT_ONCE_STATIC_INIT
#else
#include <pthread.h>
typedef pthread_once_t		onetime_t;
#define ONETIME_INIT		PTHREAD_ONCE_INIT
#endif

typedef void (*onetime_routine)(void);

#if defined(OS_WINDOWS)
static BOOL CALLBACK onetime_callback(PINIT_ONCE once, PVOID param, PVOID* context)
{
	onetime_routine routine = (onetime_routine)param;
	routine();
	once, context;
	return TRUE;
}
#endif

inline int onetime_exec(onetime_t* key, onetime_routine routine)
{
#if defined(OS_WINDOWS)
	return InitOnceExecuteOnce(key, onetime_callback, routine, NULL) ? 0 : GetLastError();
#else
	return pthread_once(key, routine);
#endif
}

#endif /* !_platform_onetime_h_ */
