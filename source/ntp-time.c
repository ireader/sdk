#include "ntp-time.h"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <sys/time.h>
#endif

ntp64_t ntp64_now()
{
	unsigned int seconds;
	unsigned int fraction;

#if defined(_WIN32) || defined(_WIN64)
	FILETIME ft;
	unsigned __int64 t;
	GetSystemTimeAsFileTime((FILETIME*)&ft); // 100-nanosecond intervals since January 1, 1601 (UTC)

	t = ((unsigned __int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
	t -= 11642264611200L; // January 1, 1601 (UTC) -> January 1, 1900 (UTC).

	// 100-nanosecond -> second
	seconds = (unsigned int)(t / 10000000);

	// ns * 1000 * 2^32 / 10^12
	// 10^6 = 2^6 * 15625
	// => ns * 2^26 / (15625 * 1000)
	fraction = (unsigned int)((t % 10000000) * 0x4000000 / (15625*1000));
#else
	struct timeval tv;	
	gettimeofday(&tv, NULL);
	seconds = (unsigned int)tv.tv_sec;
	fraction = (unsigned int)(tv.tv_usec / 15625.0 * 0x4000000);
#endif

	return ((ntp64_t)seconds << 32) | fraction;
}
