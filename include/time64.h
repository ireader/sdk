#ifndef _system_time64_h_
#define _system_time64_h_

#ifdef __cplusplus
extern "C"{
#endif

#if defined(OS_WINDOWS)
	typedef unsigned __int64 time64_t;
#else
	typedef unsigned long long time64_t;
#endif

#define MIN_UTC_TIME			(0L)
#define MAX_UTC_TIME			((time64_t)0x7fff6200 * 1000)

struct tm64
{
	int year;			/* years since 1900 */
	int month;			/* months since January 1 - [0,11] */
	int wday;			/* days since Sunday - [0,6] */
	int day;			/* day of the month - [1,31] */
	int hour;			/* hours since midnight - [0,23] */
	int minute;			/* minutes after the hour - [0,59] */
	int second;			/* seconds after the minute - [0,59] */
	int millisecond;	/* milliseconds after the minute - [0,999] */
};

// Time Format Specification Fields
// Y - year
// M - month
// D - day
// h - hour
// m - minute
// s - second
// S - millisecond
//
// usage:
//	char st[32] = {0};
//	time64_t t = time64_from("%Y-%M-%D %h:%m:%s", "2013-02-01 14:58:11");
//	time64_format(t, "%Y-%M-%D %h:%m:%s", st);
//	time64_format(t, "%04Y-%02M-%02D %02h:%02m:%02s", st);
/// @return 0-error
time64_t time64_from(const char* format, const char* src);
/// @return -1-error
int time64_format(time64_t time, const char* format, char* str);


/// UTC time(like gmtime)
/// param[in] time UTC time in ms
/// param[out] tm64 GMT datetime
/// @return 0-ok, other-error
int time64_utc(time64_t time, struct tm64* tm64);

/// local time(like localtime)
/// param[in] time UTC time in ms
/// param[out] tm64 GMT datetime
/// @return 0-ok, other-error
int time64_local(time64_t time, struct tm64* tm64);

// millisecond since the Epoch, 1970-01-01 00:00:00 +0000 (UTC)
time64_t time64_now(void);

#ifdef __cplusplus
}
#endif

#endif /* !_system_time64_h_ */
