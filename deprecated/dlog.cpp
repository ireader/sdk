#include "dlog.h"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#define MODULE_LEN 128

static const char* g_logLevelDesc[] = { "D", "W", "I", "E" };

typedef struct
{
	char name[256];
	char module[MODULE_LEN];

#if !(defined(_WIN32) || defined(_WIN64))
	int pipe;
	pthread_mutex_t locker;
#endif
} dlog_context;

static dlog_context s_log;

static int system_getmodulename(const void *address, char *name, int len)
{
#if defined(_WIN32) || defined(_WIN64)
	HMODULE hModule;

	if(!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)address, &hModule))
		return GetLastError();

	if(0 == GetModuleFileNameA(hModule, name, len))
		return GetLastError();
	return 0;
#else
	int r;
	char p[512];
	char module[512];
	void* addressfrom;
	void* addressend;
	FILE* fp;

	sprintf(p, "/proc/%d/maps", getpid());

	fp = fopen(p, "r");
	if(!fp)
		return errno;

	r = -1;
	while(fgets(p, sizeof(p)-1, fp))
	{
		r = sscanf(p, "%p-%p %*s %*s %*s %*s %s", &addressfrom, &addressend, module);
		if(3 != r)
			continue;

		if(address>=addressfrom && address<addressend)
		{
			r = 0;
			memset(name, 0, len);
			strncpy(name, module, len);
			break;
		}
	}

	fclose(fp);
	return r;
#endif
}

static int dlog_getcurrentmoudulename(char* name)
{
	int r;
	char* p;

	r = system_getmodulename(&s_log, name, MODULE_LEN);
	if(0 != r)
		return r;

#if defined(_WIN32) || defined(_WIN64)
	p = strrchr(name, '\\');
	if(p)
		memmove(name, p+1, strlen(p+1));
#endif

	p = strrchr(name, '/');
	if(p)
		memmove(name, p+1, strlen(p+1));

	p = strchr(name, '.');
	if(p) *p = '\0';
	return 0;
}

static int dlog_init()
{
	memset(&s_log, 0, sizeof(s_log));
	dlog_getcurrentmoudulename(s_log.module);

#if defined(_WIN32) || defined(_WIN64)
	return 0;
#else
	struct sigaction pipesa;
	pipesa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &pipesa, 0);

	strcpy(s_log.name, "/var/log.pipe");
	s_log.pipe = -1;

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(&s_log.locker, &attr);
	pthread_mutexattr_destroy(&attr);
	return 0;
#endif
}

//static int dlog_clean()
//{
//#if defined(_WIN32) || defined(_WIN64)
//	return 0;
//#else
//	return pthread_mutex_destroy(&s_log.locker);
//#endif
//}

static int dummy = dlog_init();


#if defined(_WIN32) || defined(_WIN64)
int dlog_log_va(int level, const char* format, va_list va)
{
	SYSTEMTIME st;
	char msg[1024*4] = {0};
	
	GetSystemTime(&st);
	sprintf(msg, "%04d-%02d-%02d %02d:%02d:%02d|%s|%s|", 
		(int)st.wYear, (int)st.wMonth, (int)st.wDay,
		(int)st.wHour, (int)st.wMinute, (int)st.wSecond,
		g_logLevelDesc[((1 <= level && level <= sizeof(g_logLevelDesc)/sizeof(g_logLevelDesc[0])) ? level-1 : 0)],
		s_log.module);

	vsnprintf(msg+strlen(msg), sizeof(msg)-1-strlen(msg), format, va);
	OutputDebugStringA(msg);
	return 0;
}

#else

static int dlog_open(const char* name)
{
	assert(-1==s_log.pipe);
	s_log.pipe = open(name, O_WRONLY|O_NONBLOCK, 0);
	return -1==s_log.pipe ? errno : 0;
}

//static int dlog_close()
//{
//	if(-1 != s_log.pipe)
//	{
//		close(s_log.pipe);
//		s_log.pipe = -1;
//	}
//	return 0;
//}

int dlog_log_va(int level, const char* format, va_list va)
{
	int r;
	time_t t;
	struct tm *lt;
	char msg[1024*4] = {0};
	
	t = time(NULL);
	lt = localtime(&t);
	sprintf(msg, "%04d-%02d-%02d %02d:%02d:%02d|%s|%s|",
		lt->tm_year+1900,	// year
		lt->tm_mon+1,		// month
		lt->tm_mday,		// day
		lt->tm_hour,		// hour
		lt->tm_min,			// minute
		lt->tm_sec,			// second
		g_logLevelDesc[((1 <= level && level <= sizeof(g_logLevelDesc) / sizeof(g_logLevelDesc[0])) ? level-1 : 0) ],
		s_log.module);

	vsnprintf(msg+strlen(msg), sizeof(msg)-1-strlen(msg), format, va);
	
	pthread_mutex_lock(&s_log.locker);
	if(-1 == s_log.pipe)
	{
		if(0 != dlog_open(s_log.name))
		{
			pthread_mutex_unlock(&s_log.locker);
			return -1;
		}
	}
	
	r =  write(s_log.pipe, msg, strlen(msg));
	pthread_mutex_unlock(&s_log.locker);
	return r;
}
#endif

int dlog_log(int level, const char* format, ...)
{
	int r;
	va_list va;

	va_start(va, format);
	r = dlog_log_va(level, format, va);
	va_end(va);
	return r;
}

int dlog_setmodule(const char* module)
{
	memset(s_log.module, 0, sizeof(s_log.module));
	strncpy(s_log.module, module, sizeof(s_log.module)-1);
	return 0;
}

int dlog_setpath(const char* name)
{
	memset(s_log.name, 0, sizeof(s_log.name));
	strncpy(s_log.name, name, sizeof(s_log.name)-1);
	return 0;
}
