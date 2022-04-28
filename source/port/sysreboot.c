#include "port/system.h"

#if defined(OS_WINDOWS)
#include <Windows.h>
#endif
#include <stdlib.h>

#if defined(OS_WINDOWS)
int windows_exit(int action)
{
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;
	OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof(info);
	GetVersionEx(&info);
	if (info.dwPlatformId == VER_PLATFORM_WIN32_NT)
	{
		if(OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &hToken))
		{
			LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
			tkp.PrivilegeCount = 1;
			tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
		}
	}

	ExitWindowsEx(action, 0);
	return GetLastError();
}
#endif

int system_reboot(void)
{
#if defined(OS_WINDOWS)
	return windows_exit(EWX_REBOOT);
#else
	return system("reboot");
#endif
}

int system_shutdown(void)
{
#if defined(OS_WINDOWS)
	return windows_exit(EWX_SHUTDOWN);
#else
	return system("halt");
#endif
}
