#include "http-header-expires.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64) || defined(OS_WINDOWS)
#define strcasecmp _stricmp
#endif

/*
3.3 Date/Time Formats (p15)
HTTP-date	= rfc1123-date | rfc850-date | asctime-date
rfc1123-date= wkday "," SP date1 SP time SP "GMT"
rfc850-date	= weekday "," SP date2 SP time SP "GMT"
asctime-date= wkday SP date3 SP time SP 4DIGIT
date1		= 2DIGIT SP month SP 4DIGIT ; day month year (e.g., 02 Jun 1982)
date2		= 2DIGIT "-" month "-" 2DIGIT ; day-month-year (e.g., 02-Jun-82)
date3		= month SP ( 2DIGIT | ( SP 1DIGIT )) ; month day (e.g., Jun 2)
time		= 2DIGIT ":" 2DIGIT ":" 2DIGIT ; 00:00:00 - 23:59:59
wkday		= "Mon" | "Tue" | "Wed" | "Thu" | "Fri" | "Sat" | "Sun"
weekday		= "Monday" | "Tuesday" | "Wednesday" | "Thursday" | "Friday" | "Saturday" | "Sunday"
month		= "Jan" | "Feb" | "Mar" | "Apr" | "May" | "Jun" | "Jul" | "Aug" | "Sep" | "Oct" | "Nov" | "Dec"

Sun, 06 Nov 1994 08:49:37 GMT ; RFC 822, updated by RFC 1123
Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
Sun Nov 6 08:49:37 1994 ; ANSI C's asctime() format
*/

static int rfc_month(const char* month)
{
	int i;
	static const char* s_months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	for (i = 0; i < sizeof(s_months) / sizeof(s_months[0]); i++)
	{
		if (0 == strcasecmp(month, s_months[i]))
			return i;
	}
	return -1;
}

static int rfc_weekday(const char* weekday)
{
	int i;
	static const char* s_weekdays[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
	for (i = 0; i < sizeof(s_weekdays) / sizeof(s_weekdays[0]); i++)
	{
		if (0 == strcasecmp(weekday, s_weekdays[i]))
			return i;
	}
	return -1;
}

static int rfc_wkday(const char* wkday)
{
	int i;
	static const char* s_wkdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	for (i = 0; i < sizeof(s_wkdays) / sizeof(s_wkdays[0]); i++)
	{
		if (0 == strcasecmp(wkday, s_wkdays[i]))
			return i;
	}
	return -1;
}

int http_header_expires(const char* field, struct tm* tm)
{
	char month[16];
	char weekday[16];
	int year, day;

	// RFC822 Sun, 06 Nov 1994 08:49:37 GMT
	if (7 == sscanf(field, "%3s, %d %3s %d %d:%d:%d GMT", weekday, &day, month, &year, &tm->tm_hour, &tm->tm_min, &tm->tm_sec))
	{
		tm->tm_wday = rfc_wkday(weekday);
	}
	// RFC850 Sunday, 06-Nov-94 08:49:37 GMT
	else if (7 == sscanf(field, "%9[^, ], %d-%3s-%d %d:%d:%d GMT", weekday, &day, month, &year, &tm->tm_hour, &tm->tm_min, &tm->tm_sec))
	{
		year += year < 70 ? 2000 : 1900;
		tm->tm_wday = rfc_weekday(weekday);
	}
	// ANSI C's asctime() Sun Nov 6 08:49:37 1994
	else if (7 == sscanf(field, "%3s %3s %d %d:%d:%d %d GMT", weekday, month, &day, &tm->tm_hour, &tm->tm_min, &tm->tm_sec, &year))
	{
		tm->tm_wday = rfc_wkday(weekday);
	}
	else
	{
		return -1;
	}

	tm->tm_year = year - 1900;
	tm->tm_mday = day;
	tm->tm_mon = rfc_month(month);
	return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
void http_header_expires_test(void)
{
	struct tm tm;
	memset(&tm, 0, sizeof(tm));

	assert(0 == http_header_expires("Sun, 06 Nov 1994 08:49:37 GMT", &tm));
	assert(94==tm.tm_year && 10==tm.tm_mon && 6==tm.tm_mday && 8==tm.tm_hour && 49==tm.tm_min && 37 == tm.tm_sec && 0 == tm.tm_wday);

	assert(0 == http_header_expires("Sunday, 06-Nov-94 08:49:37 GMT", &tm));
	assert(94 == tm.tm_year && 10 == tm.tm_mon && 6 == tm.tm_mday && 8 == tm.tm_hour && 49 == tm.tm_min && 37 == tm.tm_sec && 0 == tm.tm_wday);

	assert(0 == http_header_expires("Sun Nov 6 08:49:37 1994", &tm));
	assert(94 == tm.tm_year && 10 == tm.tm_mon && 6 == tm.tm_mday && 8 == tm.tm_hour && 49 == tm.tm_min && 37 == tm.tm_sec && 0 == tm.tm_wday);
}
#endif
