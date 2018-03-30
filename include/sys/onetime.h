#ifndef _platform_onetime_h_
#define _platform_onetime_h_

// One-Time Initialization
// https://msdn.microsoft.com/en-us/library/aa363808(VS.85).aspx
// https://msdn.microsoft.com/en-us/library/ms686934(VS.85).aspx
// https://linux.die.net/man/3/pthread_once

#if defined(OS_WINDOWS)
#include <Windows.h>
#if(_WIN32_WINNT >= 0x0600) // vista
typedef INIT_ONCE			onetime_t;
#define ONETIME_INIT		INIT_ONCE_STATIC_INIT
#else
typedef LONG			    onetime_t;
#define ONETIME_INIT		0
#endif
#else
#include <pthread.h>
typedef pthread_once_t		onetime_t;
#define ONETIME_INIT		PTHREAD_ONCE_INIT
#endif

typedef void (*onetime_routine)(void);

#if defined(OS_WINDOWS)
static BOOL CALLBACK onetime_callback(PINIT_ONCE once, PVOID param, PVOID* context)
{
	onetime_routine* routine = (onetime_routine*)param;
	(*routine)();
	once, context;
	return TRUE;
}
#endif

static inline int onetime_exec(onetime_t* key, onetime_routine routine)
{
#if defined(OS_WINDOWS)
#if(_WIN32_WINNT < 0x0600) // vista
    for (;;)
    {
        switch (InterlockedCompareExchange(key, 2, 0))
        {
        case 0: // init
            routine();
            InterlockedExchange(key, 1);
            return 0;

        case 1: // finished
            return 0;

        case 2: // running
            SwitchToThread(); // sleep(0)
            break;

        default:
            assert(0);
            return -1;
        }
    }
    return 0;
#else
	// http://stackoverflow.com/questions/12358843/why-are-function-pointers-and-data-pointers-incompatible-in-c-c
	return InitOnceExecuteOnce(key, onetime_callback, &routine, NULL) ? 0 : GetLastError();
#endif
#else
	return pthread_once(key, routine);
#endif
}

#endif /* !_platform_onetime_h_ */
