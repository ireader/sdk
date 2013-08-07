#ifndef _system_time64_h_
#define _system_time64_h_

#ifdef __cplusplus
extern "C"{
#endif

#if defined(_WIN32) || defined(_WIN64)
	typedef __int64 time64_t;
#else
	typedef long long time64_t;
#endif

#define MIN_UTC_TIME			(0L)
#define MAX_UTC_TIME			((time64_t)0x7fff6200 * 1000)

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

time64_t time64_from(const char* format, const char* src);

int time64_format(time64_t time, const char* format, char* str);

time64_t time64_now(void); // UTC millisecond

#ifdef __cplusplus
}
#endif

#endif /* !_system_time64_h_ */
