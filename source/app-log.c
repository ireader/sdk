#include "app-log.h"
#include "time64.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#if defined(OS_WINDOWS)
#include <Windows.h>
#elif defined(OS_LINUX)
#include <pthread.h>
#include <syslog.h>
#if defined(OS_ANDROID)
#include <android/log.h>
#endif
#endif

//static const char* g_logLevelDesc[] = { "EMERG", "ALERT", "CRIT", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG" };
static const char* g_logLevelDesc[] = { "X", "A", "C", "E", "W", "N", "I", "D" };
static const char g_month[][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

#if defined(OS_LINUX)
static void app_log_init(void)
{
	//char name[256] = { 0 };
	//readlink("/proc/self/exe", name, sizeof(name)-1);
	openlog(NULL, LOG_PID, LOG_USER /*| LOG_LOCAL0*/);
}
#endif

static void app_log_syslog(int level, const char* format, va_list args)
{
#if defined(OS_WINDOWS)
	int n;
	struct tm64 t;
	static char s_log[1024 * 4];
	time64_local(time64_now(), &t);
	n = snprintf(s_log, sizeof(s_log), "%02d:%02d.%03d", t.minute, t.second, t.millisecond);
	vsnprintf(s_log + n, sizeof(s_log) - n - 1, format, args);
	OutputDebugStringA(s_log);
	(void)level;
#elif defined(OS_ANDROID)
	static int s_level[] = { LOG_NOTICE, LOG_NOTICE , LOG_NOTICE, LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_CRIT, LOG_EMERG };
	assert(sizeof(s_level) / sizeof(s_level[0]) == ANDROID_LOG_SILENT + 1);
	__android_log_vprint(s_level[level % (ANDROID_LOG_SILENT + 1)], "android", format, args); 
#elif defined(OS_LINUX)
	static pthread_once_t s_onetime = PTHREAD_ONCE_INIT;
	pthread_once(&s_onetime, app_log_init);
	vsyslog(level, format, args);
#else
	#pragma message("---------error---------");
#endif
}

static void app_log_print(int level, const char* format, va_list args)
{
	struct tm64 t;
	char timestr[64] = { 0 };

	time64_local(time64_now(), &t);
	snprintf(timestr, sizeof(timestr), "%s-%02d %02d:%02d:%02d.%03d",
		/*t.year+1900,*/ g_month[t.month % 12], t.day, t.hour, t.minute, t.second, t.millisecond);

	printf("%s|%s|", timestr, g_logLevelDesc[(LOG_EMERG <= level && level <= LOG_DEBUG) ? level : LOG_DEBUG]);
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
