#include "app-log.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#if defined(OS_WINDOWS)
#include <Windows.h>
#else
#include <sys/time.h>
#include <pthread.h>
#include <syslog.h>
#if defined(OS_ANDROID)
#include <android/log.h>
#endif
#endif

//static const char s_month[][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
//static const char* s_level_tag[] = { "EMERG", "ALERT", "CRIT", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG" };
static const char* s_level_tag[] = { "X", "A", "C", "E", "W", "N", "I", "D" };

#define LOG_LEVEL(level) ((LOG_EMERG <= level && level <= LOG_DEBUG) ? level : LOG_DEBUG)

#if !defined(OS_WINDOWS) && !defined(OS_ANDROID)
static void app_log_init(void)
{
	//char name[256] = { 0 };
	//readlink("/proc/self/exe", name, sizeof(name)-1);
	openlog(NULL, LOG_PID, LOG_USER /*| LOG_LOCAL0*/);
}
#endif

/// @return time format string, e.g. 00:00:00.000|
static int app_log_time(char timestr[], unsigned int bytes)
{
#if defined(OS_WINDOWS)
	SYSTEMTIME t;
	GetLocalTime(&t);
	return snprintf(timestr, bytes, "%02hu:%02hu:%02hu.%03hu|", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
#else
	struct tm t;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &t);
	return snprintf(timestr, bytes, "%02d:%02d:%02d.%03d|", (int)t.tm_hour, (int)t.tm_min, (int)t.tm_sec, (int)(tv.tv_usec / 1000) % 1000);
#endif
	//return snprintf(timestr, sizeof(timestr), "%s-%02d %02d:%02d:%02d.%03d|", /*t.year+1900,*/ s_month[t.month % 12], t.day, t.hour, t.minute, t.second, t.millisecond);
}

static void app_log_syslog(int level, const char* format, va_list args)
{
#if defined(OS_WINDOWS)
	int n = 0;
	char log[1024 * 4];
#if defined(_DEBUG) || defined(DEBUG)
	n += app_log_time(log + n, sizeof(log) - n - 1);
#endif
	n += snprintf(log + n, sizeof(log) -n - 1, "%s|", s_level_tag[LOG_LEVEL(level)]);
	vsnprintf(log + n, sizeof(log) - n - 1, format, args);
	OutputDebugStringA(log);
#elif defined(OS_ANDROID)
	static int s_level[] = { ANDROID_LOG_FATAL/*emerg*/, ANDROID_LOG_FATAL/*alert*/, ANDROID_LOG_FATAL/*critical*/, ANDROID_LOG_ERROR/*error*/, ANDROID_LOG_WARN/*warning*/, ANDROID_LOG_INFO/*notice*/, ANDROID_LOG_INFO/*info*/, ANDROID_LOG_DEBUG/*debug*/ };
	__android_log_vprint(s_level[level % 8], "android", format, args);
#else
	static pthread_once_t s_onetime = PTHREAD_ONCE_INIT;
	pthread_once(&s_onetime, app_log_init);
	vsyslog(level, format, args);
#endif
}

static void app_log_print(int level, const char* format, va_list args)
{
	char timestr[65] = { 0 };
	app_log_time(timestr, sizeof(timestr) - 1);
	printf("%s%s|", timestr, s_level_tag[LOG_LEVEL(level)]);
	vprintf(format, args);
}

static int s_syslog_level = LOG_INFO;
void app_log_setlevel(int level)
{
	s_syslog_level = level;
}

void app_log(int level, const char* format, ...)
{
	va_list args;

	va_start(args, format);
	app_log_print(level, format, args);
	va_end(args);

	if (level <= s_syslog_level)
	{
		va_start(args, format);
		app_log_syslog(level, format, args);
		va_end(args);
	}
}
