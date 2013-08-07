#include "time64.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
typedef SYSTEMTIME system_time_t;

#else
#include <time.h>
#include <sys/time.h>
typedef unsigned short WORD;
typedef struct _system_time_t
{
	WORD wYear;			// [1601-30827]
	WORD wMonth;		// [1-12]
	WORD wDayOfWeek;	// [0-6] 0-Sunday
	WORD wDay;			// [1-31]
	WORD wHour;			// [0-23]
	WORD wMinute;		// [0-59]
	WORD wSecond;		// [0-59]
	WORD wMilliseconds; // [0-999]
} system_time_t;
#endif

#define TIME64_VALID_YEAR(v)			((v)>=1970 && (v)<=9999)
#define TIME64_VALID_MONTH(v)			((v)>=1 && (v)<=12)
#define TIME64_VALID_DAY(v)				((v)>=1 && (v)<=31)
#define TIME64_VALID_HOUR(v)			((v)>=0 && (v)<=23)
#define TIME64_VALID_MINUTE_SECOND(v)	((v)>=0 && (v)<=59)
#define TIME64_VALID_MILLISECOND(v)		((v)>=0 && (v)<=999)
#define TIME64_VALID_WEEKDAY(v)			((v)>=0 && (v)<=6)

#define TIME64_VALID_SYSTEM_TIME(st)	(TIME64_VALID_YEAR(st.wYear) \
											&& TIME64_VALID_MONTH(st.wMonth) \
											&& TIME64_VALID_DAY(st.wDay) \
											&& TIME64_VALID_HOUR(st.wHour) \
											&& TIME64_VALID_MINUTE_SECOND(st.wMinute) \
											&& TIME64_VALID_MINUTE_SECOND(st.wSecond) \
											&& TIME64_VALID_MILLISECOND(st.wMilliseconds))

static char* print_value(char padding, int width, WORD value, char* output)
{
	int n;
	char v[16] = {0};
	sprintf(v, "%u", (unsigned int)value);

	// fill padding
	n = strlen(v);
	if(n < width)
		memset(output, padding, width-n);

	strcpy(output+(n<width?width-n:0), v);
	assert((int)strlen(output)==(n<width?width:n));
	return output+(n<width?width:n);
}

static int time64_printf(const system_time_t* tm, const char* format, char* output)
{
	char padding;
	int width;
//	int precision;
	const char* p;

	for(p=format; p && *p; ++p)
	{
		if('%' != *p)
		{
			*output++ = *p;
			continue;
		}

		++p; // skip '%'

		// escape
		if('%' == *p)
		{
			*output++ = '%';
			continue;
		}

		// flag
		for(padding=' '; strchr("0-+# ", *p); ++p)
		{
			padding = *p;
		}

		// width
		for(width=0; '1'<=*p && '9'>=*p; ++p)
		{
			width = width*10 + (*p - '0');
		}

		// precision
		if('.' == *p)
		{
			assert(0);
			++p;
			// TODO...
		}

		// type
		switch(*p)
		{
		case 'Y': // year
			output = print_value(padding, width, tm->wYear, output);
			break;

		case 'M': // month
			output = print_value(padding, width, tm->wMonth, output);
			break;

		case 'D': // day
			output = print_value(padding, width, tm->wDay, output);
			break;

		case 'h': // houre
			output = print_value(padding, width, tm->wHour, output);
			break;

		case 'm': // minute
			output = print_value(padding, width, tm->wMinute, output);
			break;

		case 's': // second
			output = print_value(padding, width, tm->wSecond, output);
			break;

		case 'S': // millisecond
			output = print_value(padding, width, tm->wMilliseconds, output);
			break;

		case 'y': // 2012 -> 12
			output = print_value(padding, width, tm->wYear%100, output);
			break;

		default:
			assert(0);
		}
	}
	return 0;
}

static const char* scan_value(int asterisk, int width, WORD* value, const char* src)
{
	int i;
	unsigned int n;

	for(i=0, n=0; src && *src && (0==width||i<width); ++i,++src)
	{
		if(*src < '0' || *src > '9')
			break;

		n = n*10 + *src - '0';
	}

	if(1 != asterisk)
		*value = (WORD)n;

	return src;
}

// time64_scanf("%Y-%M-%D %h:%m:%s.%S", "2013-01-31 13:52:01.123")
static int time64_scanf(system_time_t* tm, const char* format, const char* src)
{
	const char* p;
	int asterisk;
	int width;

	for(p=format; p && *p && src && *src; ++p)
	{
		if('%' != *p)
		{
			++src;
			continue;
		}

		++p; // skip '%'

		// The field is scanned but not stored
		asterisk = 0;
		if('*' == *p)
		{
			asterisk = 1;
			++p;
		}

		// width
		for(width=0; '0'<=*p && '9'>=*p; ++p)
		{
			width = width*10 + (*p - '0');
		}

		// type
		switch(*p)
		{
		case 'Y': // year
			src = scan_value(asterisk, width, &tm->wYear, src);
			break;

		case 'M': // month
			src = scan_value(asterisk, width, &tm->wMonth, src);
			break;

		case 'D': // day
			src = scan_value(asterisk, width, &tm->wDay, src);
			break;

		case 'h': // hour
			src = scan_value(asterisk, width, &tm->wHour, src);
			break;

		case 'm': // minute
			src = scan_value(asterisk, width, &tm->wMinute, src);
			break;

		case 's': // second
			src = scan_value(asterisk, width, &tm->wSecond, src);
			break;

		case 'S': // millisecond
			src = scan_value(asterisk, width, &tm->wMilliseconds, src);
			break;

		case 'y': // 2012 -> 12
			src = scan_value(asterisk, width, &tm->wYear, src); // TODO: get year
			break;

		default:
			assert(0);
		}
	}
	return 0;
}

int time64_format(time64_t time, const char* format, char* str)
{
	system_time_t st;

#if defined(_WIN32) || defined(_WIN64)
	FILETIME ft;

	time += 11644473600000L; // January 1, 1601 (UTC) -> January 1, 1970 (UTC).
	time *= 10000; // millisecond -> nanosecond

	ft.dwHighDateTime = (time>>32) & 0xFFFFFFFF;
	ft.dwLowDateTime = time & 0xFFFFFFFF;
	FileTimeToSystemTime(&ft, &st);
#else
	time_t t;
	struct tm* lt;

	t = time / 1000;
	lt = localtime(&t);

	memset(&st, 0, sizeof(st));
	st.wYear = (WORD)lt->tm_year + 1900;
	st.wMonth = (WORD)lt->tm_mon + 1;
	st.wDay = (WORD)lt->tm_mday;
	st.wHour = (WORD)lt->tm_hour;
	st.wMinute = (WORD)lt->tm_min;
	st.wSecond = (WORD)lt->tm_sec;
	st.wMilliseconds = time % 1000;
#endif

	if(!TIME64_VALID_SYSTEM_TIME(st))
		return -1;
	return time64_printf(&st, format, str);
}

time64_t time64_from(const char* format, const char* src)
{
	time64_t v;
	system_time_t st;

#if defined(_WIN32) || defined(_WIN64)
	FILETIME ft;

	memset(&st, 0, sizeof(st));
	st.wYear = 1970;
	st.wMonth = 1;
	st.wDay = 1;
	time64_scanf(&st, format, src);
	if(!TIME64_VALID_SYSTEM_TIME(st))
		return -1;

	SystemTimeToFileTime(&st, &ft);

	v = (((__int64)ft.dwHighDateTime << 32) | (__int64)ft.dwLowDateTime) / 10000; // to ms
	v -= 11644473600000L; // January 1, 1601 (UTC) -> January 1, 1970 (UTC).

#else
	struct tm t;

	memset(&st, 0, sizeof(st));
	st.wYear = 1900;
	st.wMonth = 1;
	time64_scanf(&st, format, src);
	if(!TIME64_VALID_SYSTEM_TIME(st))
		return -1;

	t.tm_year = st.wYear-1900;
	t.tm_mon = st.wMonth-1;
	t.tm_mday = st.wDay;
	t.tm_hour = st.wHour;
	t.tm_min = st.wMinute;
	t.tm_sec = st.wSecond;

	v = mktime(&t);
	v *= 1000;
	v += st.wMilliseconds;
#endif
	return v;
}

time64_t time64_now(void)
{
	time64_t v;
	
#if defined(_WIN32) || defined(_WIN64)
	FILETIME ft;
	GetSystemTimeAsFileTime((FILETIME*)&ft);

	v = (((__int64)ft.dwHighDateTime << 32) | (__int64)ft.dwLowDateTime) / 10000; // to ms
	v -= 11644473600000L; // January 1, 1601 (UTC) -> January 1, 1970 (UTC).
#else
	struct timeval tv;	
	gettimeofday(&tv, NULL);
	v = tv.tv_sec*1000;
	v += tv.tv_usec / 1000;
#endif
	return v;
}
