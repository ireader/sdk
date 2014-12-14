/// RFC5905 NTPv4 => 6. Data Types
/// 1. The 64-bit timestamp format is used in packet headers and other
///    places with limited word size. It includes a 32-bit unsigned seconds
///    field spanning 136 years and a 32-bit fraction field resolving 232
///    picoseconds. 232 = 10^12 / (1<<32) (p13)
/// 2. In the date and timestamp formats, the prime epoch, or base date of
///    era 0, is 0 h 1 January 1900 UTC, when all bits are zero. (p13)

#include "ntp-time.h"

#if defined(OS_WINDOWS)
#include <Windows.h>
#else
#include <sys/time.h>
#endif

ntp64_t ntp64_now(void)
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
	fraction = (unsigned int)((t % 10000000) / 156250.0 * 0x4000000);
#else
	// seconds and microseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
	struct timeval tv;	
	gettimeofday(&tv, 0);
	seconds = (unsigned int)(tv.tv_sec + 0x83AA7E80); // 1/1/1970 -> 1/1/1900
	fraction = (unsigned int)(tv.tv_usec / 15625.0 * 0x4000000);
#endif

	return ((ntp64_t)seconds << 32) | fraction;
}
