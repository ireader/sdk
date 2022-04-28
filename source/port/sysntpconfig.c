#include "port/system.h"

#if defined(OS_LINUX)
#include <sys/utsname.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(OS_LINUX)
int system_call(const char* call, char* output, int outputLen)
{
	int n = 0;
	char str[1024] = {0};
	strcpy(str, call);
	strcat(str, " 2>&1"); //2>&1: redirect stderr to stdout(for error message)

	FILE *fp = popen(str, "r");
	if(!fp)
		return -(int)errno;

	while(fgets(str, sizeof(str)-1, fp))
	{
		str[sizeof(str)-1] = 0;
		n = strlen(str);
		if(outputLen < n)
			break;

		outputLen -= n;
		strcat(output, str);		
	}

	pclose(fp);
	return 0;
}
#endif

int system_ntp_getstatus(int *enable)
{
#if defined(OS_LINUX)
	char str[2*1024] = {0};

	/*	MD
		NTPSERVER
		NTPSERVICE   N
		Started      N
		No NTP Process. NTP Service is not started.
	*/
	system_call("/etc/init.d/ntpservice status", str, sizeof(str));
	if(strchr(str, 'Y'))
	{
		*enable = 1;
		return 0;
	}

	/*	MD
		ntpd (pid  972) is running...
	*/
	system_call("service ntpd status", str, sizeof(str));
	if(strstr(str, "running"))
	{
		*enable = 1;
		return 0;
	}

	*enable = 0;
	return 0;
#else
	(void)enable;
	return -1;
#endif
}

int system_ntp_setenable(int enable)
{
#if defined(OS_LINUX)
	if(enable)
	{
		system("/etc/init.d/ntpservice start");	// MD
		system("service ntpd start");			// MD
	}
	else
	{
		system("/etc/init.d/ntpservice stop");
		system("service ntpd stop");
	}
	return 0;
#else
	(void)enable;
	return -1;
#endif
}
