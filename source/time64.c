#include "time64.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#if defined(OS_WINDOWS)
#include <Windows.h>
#define snprintf _snprintf

#else
#include <sys/time.h>

static time64_t utc_mktime(const struct tm *t)
{
    int mon = t->tm_mon+1, year = t->tm_year+1900;
    
    /* 1..12 -> 11,12,1..10 */
    if (0 >= (int) (mon -= 2)) {
        mon += 12;  /* Puts Feb last since it has leap day */
        year -= 1;
    }
    
    return ((((time64_t)
              (year/4 - year/100 + year/400 + 367*mon/12 + t->tm_mday) +
              year*365 - 719499
              )*24 + t->tm_hour /* now have hours */
             )*60 + t->tm_min /* now have minutes */
            )*60 + t->tm_sec; /* finally seconds */
}
#endif

#define TIME64_VALID_YEAR(v)			((v)>=0 && (v)<=9999)
#define TIME64_VALID_MONTH(v)			((v)>=0 && (v)<=11)
#define TIME64_VALID_DAY(v)				((v)>=1 && (v)<=31)
#define TIME64_VALID_HOUR(v)			((v)>=0 && (v)<=23)
#define TIME64_VALID_MINUTE_SECOND(v)	((v)>=0 && (v)<=59)
#define TIME64_VALID_MILLISECOND(v)		((v)>=0 && (v)<=999)
#define TIME64_VALID_WEEKDAY(v)			((v)>=0 && (v)<=6)

#define TIME64_VALID_SYSTEM_TIME(tm64)	(TIME64_VALID_YEAR(tm64.year) \
											&& TIME64_VALID_MONTH(tm64.month) \
											&& TIME64_VALID_DAY(tm64.day) \
											&& TIME64_VALID_HOUR(tm64.hour) \
											&& TIME64_VALID_MINUTE_SECOND(tm64.minute) \
											&& TIME64_VALID_MINUTE_SECOND(tm64.second) \
											&& TIME64_VALID_MILLISECOND(tm64.millisecond))

static char* print_value(char padding, size_t width, int value, char* output)
{
	size_t n;
	char v[16] = {0};
	sprintf(v, "%d", value);

	// fill padding
	n = strlen(v);
	if(n < width)
		memset(output, padding, width-n);

    //	strcpy(output+(n<width?width-n:0), v);
    memcpy(output+(n<width?width-n:0), v, n+1);
	assert(strlen(output)==(n<width?width:n));
	return output+(n<width?width:n);
}

static int time64_printf(const struct tm64* tm64, const char* format, char* output)
{
	char padding;
	size_t width;
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
			width = width*10 + (size_t)(*p - '0');
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
			output = print_value(padding, width, tm64->year+1900, output);
			break;

		case 'M': // month
			output = print_value(padding, width, tm64->month+1, output);
			break;

		case 'D': // day
			output = print_value(padding, width, tm64->day, output);
			break;

		case 'h': // houre
			output = print_value(padding, width, tm64->hour, output);
			break;

		case 'm': // minute
			output = print_value(padding, width, tm64->minute, output);
			break;

		case 's': // second
			output = print_value(padding, width, tm64->second, output);
			break;

		case 'S': // millisecond
			output = print_value(padding, width, tm64->millisecond, output);
			break;

		case 'y': // 2012 -> 12
			output = print_value(padding, width, (tm64->year+1900)%100, output);
			break;

		default:
			assert(0);
		}
	}
	return 0;
}

static const char* scan_value(int asterisk, int width, int* value, const char* src)
{
	int i;
	int n;

	for(i=0, n=0; src && *src && (0==width||i<width); ++i,++src)
	{
		if(*src < '0' || *src > '9')
			break;

		n = n*10 + (*src - '0');
	}

	if(1 != asterisk)
		*value = n;

	return src;
}

// time64_scanf("%Y-%M-%D %h:%m:%s.%S", "2013-01-31 13:52:01.123")
static int time64_scanf(struct tm64* tm64, const char* format, const char* src)
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
			src = scan_value(asterisk, width, &tm64->year, src);
			tm64->year -= 1900;
			break;

		case 'M': // month
			src = scan_value(asterisk, width, &tm64->month, src);
			tm64->month -= 1;
			break;

		case 'D': // day
			src = scan_value(asterisk, width, &tm64->day, src);
			break;

		case 'h': // hour
			src = scan_value(asterisk, width, &tm64->hour, src);
			break;

		case 'm': // minute
			src = scan_value(asterisk, width, &tm64->minute, src);
			break;

		case 's': // second
			src = scan_value(asterisk, width, &tm64->second, src);
			break;

		case 'S': // millisecond
			src = scan_value(asterisk, width, &tm64->millisecond, src);
			break;

		case 'y': // 2012 -> 12
			src = scan_value(asterisk, width, &tm64->year, src);
			tm64->year = (2000 + tm64->year) - 1900;
			break;

		default:
			assert(0);
		}
	}
	return 0;
}

int time64_format(time64_t time, const char* format, char* str)
{
	struct tm64 tm64;
	time64_utc(time, &tm64);

	if(!TIME64_VALID_SYSTEM_TIME(tm64))
		return -1;
	return time64_printf(&tm64, format, str);
}

time64_t time64_from(const char* format, const char* src)
{
	time64_t v;
	struct tm64 tm64;

#if defined(OS_WINDOWS)
	FILETIME ft;
	SYSTEMTIME st;

	memset(&tm64, 0, sizeof(tm64));
	time64_scanf(&tm64, format, src);
	if(!TIME64_VALID_SYSTEM_TIME(tm64))
		return 0;

	st.wYear = (WORD)(tm64.year + 1900);
	st.wMonth = (WORD)(tm64.month + 1);
	st.wDay = (WORD)tm64.day;
	st.wDayOfWeek = (WORD)tm64.wday;
	st.wHour = (WORD)tm64.hour;
	st.wMinute = (WORD)tm64.minute;
	st.wSecond = (WORD)tm64.second;
	st.wMilliseconds = (WORD)tm64.millisecond;
	SystemTimeToFileTime(&st, &ft);

	v = (((unsigned __int64)ft.dwHighDateTime << 32) | (unsigned __int64)ft.dwLowDateTime) / 10000; // to ms
	v -= 0xA9730B66800; /* 11644473600000LL */ // January 1, 1601 (UTC) -> January 1, 1970 (UTC).
#else
	struct tm t;

	memset(&tm64, 0, sizeof(tm64));
	tm64.day = 1;
	time64_scanf(&tm64, format, src);
	if(!TIME64_VALID_SYSTEM_TIME(tm64))
		return 0;

	t.tm_year = tm64.year;
	t.tm_mon = tm64.month;
	t.tm_mday = tm64.day;
	t.tm_wday = tm64.wday;
	t.tm_hour = tm64.hour;
	t.tm_min = tm64.minute;
	t.tm_sec = tm64.second;
//	v = mktime(&t);
    v = utc_mktime(&t);
    v *= 1000;
	v += tm64.millisecond;
#endif
	return v;
}

int time64_utc(time64_t time, struct tm64* tm64)
{
#if defined(OS_WINDOWS)
	FILETIME ft;
	SYSTEMTIME st;

	time += 0xA9730B66800; /* 11644473600000LL */ // January 1, 1970 (UTC) -> January 1, 1601 (UTC).
	time *= 10000; // millisecond -> nanosecond

	ft.dwHighDateTime = (time>>32) & 0xFFFFFFFF;
	ft.dwLowDateTime = time & 0xFFFFFFFF;
	FileTimeToSystemTime(&ft, &st);

	assert(st.wYear >= 1900);
	assert(st.wMonth);
	tm64->year = st.wYear - 1900;
	tm64->month = st.wMonth - 1;
	tm64->day = st.wDay;
	tm64->wday = st.wDayOfWeek;
	tm64->hour = st.wHour;
	tm64->minute = st.wMinute;
	tm64->second = st.wSecond;
	tm64->millisecond = st.wMilliseconds;
#else
	struct tm t;
	time_t seconds;

	seconds = (time_t)(time / 1000);
	gmtime_r(&seconds, &t);

	tm64->year = t.tm_year;
	tm64->month = t.tm_mon;
	tm64->day = t.tm_mday;
	tm64->wday = t.tm_wday;
	tm64->hour = t.tm_hour;
	tm64->minute = t.tm_min;
	tm64->second = t.tm_sec;
	tm64->millisecond = (int)(time % 1000);
#endif

	return 0;
}

int time64_local(time64_t time, struct tm64* tm64)
{
	struct tm t;
	time_t seconds;

	seconds = (time_t)(time / 1000);

#if defined(OS_WINDOWS)
	localtime_s(&t, &seconds);
#else
	localtime_r(&seconds, &t);
#endif

	tm64->year = t.tm_year;
	tm64->month = t.tm_mon;
	tm64->day = t.tm_mday;
	tm64->wday = t.tm_wday;
	tm64->hour = t.tm_hour;
	tm64->minute = t.tm_min;
	tm64->second = t.tm_sec;
	tm64->millisecond = (int)(time % 1000);
	return 0;
}

time64_t time64_now(void)
{
	time64_t v;

#if defined(OS_WINDOWS)
	FILETIME ft;
	GetSystemTimeAsFileTime((FILETIME*)&ft);
	v = (((__int64)ft.dwHighDateTime << 32) | (__int64)ft.dwLowDateTime) / 10000; // to ms
	v -= 0xA9730B66800; /* 11644473600000LL */ // January 1, 1601 (UTC) -> January 1, 1970 (UTC).
#else
	struct timeval tv;	
	gettimeofday(&tv, NULL);
	v = tv.tv_sec;
	v *= 1000;
	v += tv.tv_usec / 1000;
#endif
	return v;
}

#if defined(_DEBUG) || defined(DEBUG)
static time_t time64_to_gmtime(time64_t time64)
{
	return time64 / 1000;
}

static time64_t time64_from_gmtime(time_t gmt)
{
	time64_t time64 = (time64_t)gmt;
	time64 *= 1000;
	return time64;
}

void time64_test(void)
{
    time_t t;
    time64_t t64, _t64;
    struct tm *tm;
	struct tm64 tm64;
    char gmt[64];
    char utc[64];

    t64 = time64_now();
	time64_utc(t64, &tm64);

    t = time64_to_gmtime(t64);
	_t64 = time64_from_gmtime(t);
	assert(_t64/1000 == t64 / 1000);

	tm = gmtime(&t);
	assert(tm64.year==tm->tm_year && tm64.month==tm->tm_mon && tm64.day==tm->tm_mday && tm64.wday==tm->tm_wday && tm64.hour==tm->tm_hour && tm64.minute==tm->tm_min && tm64.second==tm->tm_sec);

	sprintf(gmt, "%04d-%02d-%02d %02d:%02d:%02d", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    time64_format(t64, "%04Y-%02M-%02D %02h:%02m:%02s", utc);
    assert(0 == strcmp(utc, gmt));

    _t64 = time64_from("%04Y-%02M-%02D %02h:%02m:%02s", utc);
    assert(t64/1000 == _t64/1000);
}
#endif
