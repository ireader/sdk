#include "port/system.h"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>

#else
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../deprecated/tools.h"

#if defined(_WIN32) || defined(_WIN64)
int system_gettime(char time[24])
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	sprintf(time, "%04hd-%02hd-%02hd %02hd:%02hd:%02hd.%03hd", 
		st.wYear, 
		st.wMonth, 
		st.wDay, 
		st.wHour, 
		st.wMinute, 
		st.wSecond,
		st.wMilliseconds);
	return 0;
}

int system_settime(const char* time)
{
	int n;
	SYSTEMTIME st;

	n = strlen(time);
	if(23 == n)
	{
		if(7 != sscanf(time, "%hd-%hd-%hd %hd:%hd:%hd.%hd", &st.wYear, &st.wMonth, &st.wDay, &st.wHour, &st.wMinute, &st.wSecond, &st.wMilliseconds))
			return -1;
	}
	else if(19 == n)
	{
		st.wMilliseconds = 0;
		if(6 != sscanf(time, "%hd-%hd-%hd %hd:%hd:%hd", &st.wYear, &st.wMonth, &st.wDay, &st.wHour, &st.wMinute, &st.wSecond))
			return -1;
	}
	else
	{
		return -1;
	}

	if(SetLocalTime(&st))
		return 0;
	return -(int)GetLastError();
}

int system_ntp_getserver(char* servers, int serversLen)
{
	return -1;
}

int system_ntp_setserver(const char *servers)
{
	return -1;
}

#else
int system_gettime(char time[24])
{
	struct tm* t;
	struct timeval tv;	
	gettimeofday(&tv, NULL);

	t = localtime(&tv.tv_sec);
	sprintf(time, "%04d-%02d-%02d %02d:%02d:%02d.%03d", 
		t->tm_year+1900,
		t->tm_mon+1,
		t->tm_mday,
		t->tm_hour,
		t->tm_min,
		t->tm_sec,
		(int)tv.tv_usec/1000);
	return 0;
}

int system_settime(const char* time)
{
	int n;
	struct tm t;
	struct timeval tv;

	memset(&t, 0, sizeof(t));
	n = (int)strlen(time);
	if(23 == n)
	{
		if(7 != sscanf(time, "%d-%d-%d %d:%d:%d.%d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec, &n))
			return -1;
	}
	else if(19 == n)
	{
		n = 0;
		if(6 != sscanf(time, "%d-%d-%d %d:%d:%d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec))
			return -1;
	}
	else
	{
		return -1;
	}

	t.tm_year -= 1900;
	t.tm_mon -= 1;
	
	tv.tv_usec = n*1000;
	tv.tv_sec = mktime(&t);

	return settimeofday(&tv, 0);
}

static char g_ntpconfig[512] = "/etc/ntp.conf";
int system_ntp_setconfigpath(const char* path)
{
	strcpy(g_ntpconfig, path);
	return 0;
}

int system_ntp_getconfigpath(char* path, int pathLen)
{
	strncpy(path, g_ntpconfig, pathLen-1);
	path[pathLen] = 0;
	return 0;
}

static int system_ntp_getserver_handle(const char* str, int strLen, va_list val)
{
	char *servers;
	const char *p;
	int serversLen;
	size_t n;

	if(0 == strncmp("server ", str, 7))
	{
		servers = va_arg(val, char*);
		serversLen = va_arg(val, int);

		n = strlen(servers);
		if(n > 0)
		{
			servers[n] = ';';
			servers += n+1;
			serversLen -= 1;
		}
		serversLen -= (int)n;

		p = str + 6;
		p += strcspn(p, " ");
		n = strspn(p, "\r\n ");
		if ((int)n + 1 < serversLen)
		{
			strncpy(servers, p, n);
			servers[n] = '\0';
		}
	}
	return 0;
}

int system_ntp_getserver(char* servers, int serversLen)
{
	int r;
	char content[1024*5] = {0};

	r = tools_cat(g_ntpconfig, content, sizeof(content)-1);
	r = tools_tokenline(content, system_ntp_getserver_handle, servers, serversLen); 
	return r;
}

static int system_ntp_setserver_handle(const char* str, int len, va_list val)
{
	FILE* fp;

	if(len < 1)
		return 0;

	if(strncmp(str, "server ", 7))
	{
		fp = va_arg(val, FILE*);
		fwrite(str, 1, len, fp);
	}

	return 0;
}

int system_ntp_setserver(const char *servers)
{
	int r;
	FILE* fp;
	char content[1024*5] = {0};	
	const char *line, *p;

	// read
	tools_cat(g_ntpconfig, content, sizeof(content)-1);

	fp = fopen(g_ntpconfig, "w");
	if(!fp)
		return -(int)errno;

	// update
	r = tools_tokenline(content, system_ntp_setserver_handle, fp);

	line = servers;
	while(line && *line)
	{
		line += strcspn(line, " ;");
		p = strchr(line, ';');
		if(p)
		{
			assert(p > line);
			fwrite("server ", 1, 7, fp);
			fwrite(line, 1, p-line, fp);
			fwrite("\n", 1, 1, fp);
		}
		else
		{
			sprintf(content, "server %s\n", line);
			fwrite(content, 1, strlen(content), fp);
			break;
		}
		line = p+1;
	}

	fclose(fp);
	return r;
}

#endif
