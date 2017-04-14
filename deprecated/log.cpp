#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include "sys/process.h"
#include "sys/sync.hpp"
#include "sys/path.h"
#include "log.h"

static int g_logLevel = LOG_WARNING;
static char g_logFile[255] = {0};
static const char* g_logLevelDesc[] = { "debug", "warning", "info", "error" };

static ThreadLocker g_locker;
static FILE* g_fp = NULL;

static int log_open()
{
	if(!!g_fp)
		return 0;

	if(0 == g_logFile[0])
	{
		char procname[256] = {0};
		if(0 != process_selfname(procname, sizeof(procname)))
			return errno;

		sprintf(g_logFile, "%s.%d.log", path_basename(procname), process_self());
	}

	g_fp = fopen(g_logFile, "w+");
	if(NULL == g_fp)
		return errno;
	return 0;
}

static int log_close()
{
	if(g_fp)
		fclose(g_fp);
	g_fp = NULL;
	return 0;
}

void log_log_va(int level, const char* format, va_list vl)
{
	if(level < g_logLevel)
		return;

	AutoThreadLocker locker(g_locker);
	if(NULL == g_fp)
	{
		if(0 != log_open())
			return;
	}

	time_t t = time(NULL);
	tm* gmt = gmtime(&t);
	int r = fprintf(g_fp, "%04d-%02d-%02d %02d:%02d:%02d GMT %s|",
		gmt->tm_year+1900,	// year
		gmt->tm_mon+1,		// month
		gmt->tm_mday,		// day
		gmt->tm_hour,		// hour
		gmt->tm_min,		// minute
		gmt->tm_sec,		// second
		g_logLevelDesc[((LOG_DEBUG<=level && level<=LOG_ERROR) ? level : LOG_DEBUG) - 1]);

	r = vfprintf(g_fp, format, vl);
	if(r < 0)
	{
		log_close();
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
///
///  log level
///
//////////////////////////////////////////////////////////////////////////
int log_getlevel()
{
	return g_logLevel;
}

void log_setlevel(int level)
{
	g_logLevel = level;
}

//////////////////////////////////////////////////////////////////////////
///
///  log path
///
//////////////////////////////////////////////////////////////////////////
const char* log_getfile()
{
	return g_logFile;
}

void log_setfile(const char* file)
{
	if(NULL==file || 0==*file)
		return;
	strncpy(g_logFile, file, sizeof(g_logFile)-1);

	log_close();
}

//////////////////////////////////////////////////////////////////////////
///
///  write log
///
//////////////////////////////////////////////////////////////////////////
void log_debug(const char* format, ...)
{
	va_list vl;
	va_start(vl, format);
	log_log_va(LOG_DEBUG, format, vl);
	va_end(vl);
}

void log_warning(const char* format, ...)
{
	va_list vl;
	va_start(vl, format);
	log_log_va(LOG_WARNING, format, vl);
	va_end(vl);
}

void log_info(const char* format, ...)
{
	va_list vl;
	va_start(vl, format);
	log_log_va(LOG_INFO, format, vl);
	va_end(vl);
}

void log_error(const char* format, ...)
{
	va_list vl;
	va_start(vl, format);
	log_log_va(LOG_ERROR, format, vl);
	va_end(vl);
}

void log_log(int level, const char* format, ...)
{
	va_list vl;
	va_start(vl, format);
	log_log_va(level, format, vl);
	va_end(vl);
}

void log_flush()
{
	fflush(g_fp);
}
