#include "app-log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#if defined(OS_WINDOWS)
#define THREAD_LOCAL static __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define THREAD_LOCAL static __thread
#else
#define THREAD_LOCAL 
#endif

#if !defined(OS_ANDROID) && (defined(OS_LINUX) || defined(OS_WINDOWS) || defined(OS_MAC))
#define N_LOG_BUFFER 1024 * 8  // TLS/SDP
#else
#define N_LOG_BUFFER 1024 * 2
#endif

static app_log_provider s_provider;
static void* s_provider_param;

//static const char s_month[][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
//static const char* s_level_tag[] = { "EMERG", "ALERT", "CRIT", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG" };
static const char* s_level_tag[] = { "X", "A", "C", "E", "W", "N", "I", "D" };
static const char* s_level_color_default[] = { "\033[0m", "\033[0;35m", "\033[0;34m", "\033[0;33m", "\033[0;31m", "\033[0;33m", "\033[0;37m", "\033[0;32m", "\033[0m", };
static const char* s_level_color_none[] = { "", "", "", "", "", "", "", "", "", };
static const char** s_level_color = s_level_color_none;

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
	return snprintf(timestr, bytes, "%04hu-%02hu-%02huT%02hu:%02hu:%02hu.%03hu|", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
#else
	struct tm t;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &t);
	return snprintf(timestr, bytes, "%04d-%02d-%02dT%02d:%02d:%02d.%03d|", (int)t.tm_year + 1900, (int)t.tm_mon+1, (int)t.tm_mday, (int)t.tm_hour, (int)t.tm_min, (int)t.tm_sec, (int)(tv.tv_usec / 1000) % 1000);
#endif
	//return snprintf(timestr, sizeof(timestr), "%s-%02d %02d:%02d:%02d.%03d|", /*t.year+1900,*/ s_month[t.month % 12], t.day, t.hour, t.minute, t.second, t.millisecond);
}

static void app_log_syslog(int level, const char* format, va_list args)
{
#if defined(OS_WINDOWS)
	int n = 0;
	THREAD_LOCAL char log[N_LOG_BUFFER];
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
	int n;
	char timestr[65];
	THREAD_LOCAL char log[N_LOG_BUFFER];
	app_log_time(timestr, sizeof(timestr));
	n = vsnprintf(log, sizeof(log) - 1, format, args);
	printf("%s%s%s|%.*s%s", s_level_color[LOG_LEVEL(level) + 1], timestr, s_level_tag[LOG_LEVEL(level)], n, log, s_level_color[0]);

	if (s_provider)
	{
		strcat(timestr, s_level_tag[LOG_LEVEL(level)]);
		s_provider(s_provider_param, timestr, log, n);
	}
}

static int s_syslog_level = LOG_INFO;
void app_log_setlevel(int level)
{
	s_syslog_level = level;
}

void app_log(int level, const char* format, ...)
{
	va_list args;

	if (level <= s_syslog_level)
	{
		va_start(args, format);
		app_log_print(level, format, args);
		va_end(args);

		if (!s_provider)
		{
			va_start(args, format);
			app_log_syslog(level, format, args);
			va_end(args);
		}
	}
}

void app_log_setcolor(int enable)
{
	s_level_color = enable ? s_level_color_default : s_level_color_none;
}

void app_log_setprovider(app_log_provider provider, void* param)
{
	s_provider = provider;
	s_provider_param = param;
}
