#include "port/process.h"
#include "sys/system.h"
#include "sys/path.h"
#include "../deprecated/tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if defined(_WIN32)
#undef UNICODE
#include <Tlhelp32.h>
#else
#include <dirent.h>
#include <linux/param.h>
#endif

static int on_process(void* param, const char* name, pid_t pid)
{
	const char* p;
	const char* process = (const char*)param;

	p = path_basename(name);
#if defined(OS_WINDOWS)
	if(0 == _stricmp(process, p))
#else
	if(0 == strcmp(process, p))
#endif
	{
		process_kill(pid);
	}
	return 0;
}

int process_kill_all(const char* name)
{
	const char* p = path_basename(name);
	return process_list(on_process, (void*)p);
}

int process_list(fcb_process_list callback, void* param)
{
#if defined(_WIN32)
	//DWORD size = 0;
	//DWORD pids[2048] = {0};
	//TCHAR szProcessName[MAX_PATH] = {0};
	//if(!EnumProcesses(pids, sizeof(pids), &size))
	//	return (int)GetLastError();

	//for(DWORD i=0; i<size/sizeof(DWORD); i++)
	//{
	//	HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pids[i]);
	//	if(!handle)
	//		continue;

	//	if(GetModuleBaseName(handle, NULL, szProcessName, sizeof(szProcessName)/sizeof(szProcessName[0])))
	//	{
	//		OutputDebugStr(szProcessName);
	//		if(0 == stricmp(filename, szProcessName))
	//			TerminateProcess(handle, 0);
	//	}
	//	CloseHandle(handle);
	//}
	//return 0;

	int r;
	HANDLE h;
	PROCESSENTRY32 pe32;
	
	h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(INVALID_HANDLE_VALUE == h)
		return -(int)GetLastError();

	pe32.dwSize = sizeof(pe32);
	if(!Process32First(h, &pe32))
		return -(int)GetLastError();

	do
	{
		r = callback(param, pe32.szExeFile, pe32.th32ProcessID);
		if(0 != r)
			return r;
	} while(Process32Next(h, &pe32));

	return 0;

#else
	int r, pid;
	DIR* dir;
	struct dirent* p;
	char filename[5*1024] = {0};

	r = -1;
	dir = opendir("/proc");
	if(!dir)
		return -(int)errno;

	for(p=readdir(dir); p; p=readdir(dir))
	{
		if(0==strcmp(p->d_name, ".")||0==strcmp(p->d_name, ".."))
		{
			continue;
		}
		else if(p->d_type == DT_DIR)
		{
			pid = atoi(p->d_name);
			if(0 == pid)
				continue;

			r = process_name(pid, filename, sizeof(filename));
			if(0 == r)
			{
				r = callback(param, filename, pid);
				if(0 != r)
					break;
			}
		}
	}

	closedir(dir);
	return r;
#endif
}

#if defined(_WIN32)
static ULARGE_INTEGER FILETIME2UINT64(const FILETIME* ft)
{
	ULARGE_INTEGER v;
	v.HighPart = ft->dwHighDateTime;
	v.LowPart = ft->dwLowDateTime;
	return v;
}
#else
static int system_uptime(float* time)
{
	int r;
	float idletime;
	char p[256] = {0};
	
	sprintf(p, "/proc/uptime");
	r = tools_cat(p, p, sizeof(p));
	if(r < 0)
		return r;

	if(2 != sscanf(p, "%f %f", time, &idletime))
		return -(int)EINVAL;
	return 0;
}
#endif

int process_time(pid_t pid, time64_t* createTime, time64_t* kernelTime, time64_t* userTime)
{
#if defined(_WIN32)
	HANDLE handle;
	FILETIME ftCreateTime, ftExitTime, ftKernelTime, ftUserTime;
	SYSTEMTIME stime;
	FILETIME ftime;

	ULONGLONG rt;
	ULARGE_INTEGER kt, ut;

	memset(&ftUserTime, 0xCC, sizeof(ftUserTime));
	handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
	if(!handle)
		return (int)GetLastError();
	
	if(!GetProcessTimes(handle, &ftCreateTime, &ftExitTime, &ftKernelTime, &ftUserTime))
	{
		CloseHandle(handle);
		return (int)GetLastError();
	}
	CloseHandle(handle);
	
	GetSystemTime(&stime);
	SystemTimeToFileTime(&stime, &ftime);

	kt = FILETIME2UINT64(&ftime);
	ut = FILETIME2UINT64(&ftCreateTime);
	rt = kt.QuadPart > ut.QuadPart ? kt.QuadPart-ut.QuadPart : 0; // for resolution problem
	kt = FILETIME2UINT64(&ftKernelTime);
	ut = FILETIME2UINT64(&ftUserTime);

	*createTime = rt/10000; // nanosecond -> millisecond
	*userTime = ut.QuadPart/10000;
	*kernelTime = kt.QuadPart/10000;
	return 0;
#else
	char content[2*1024] = {0};

	int r;
	unsigned long int utime, stime;
	unsigned long long starttime;
	float uptime = 0.0f;

	sprintf(content, "/proc/%d/stat", pid);
	r = tools_cat(content, content, sizeof(content));
	if(r < 0)
		return r;

	// linux: man proc
	// cat proc/self/stat
	// (1-pid-%d, 2-filename-%s, 3-state-%c, 4-ppid-%d, 5-pgrp-%d, 
	//	6-session-%d, 7-tty_nr-%d, 8-tpgid-%d, 9-flags-%u, 10-minflt-%lu, 
	//	11-cminflt-%lu, 12-majflt-%lu, 13-cmajflt-%lu, 14-utime-%lu, 15-stime-%lu, 
	//	16-cutime-%ld, 17-cstime-%ld, 18-priority-%ld, 19-nice-%ld, 20-num_threads-%ld, 
	//	21-itrealvalue-%ld, 22-starttime-%llu, 23-vsize-%lu, 24-rss-%ld, 25-rsslim-%lu, 
	//	26-startcode-%lu, 27-endcode-%lu, 28-startstack-%lu, 29-kstkesp-%lu, 30-kstkeip-%lu, 
	//	31-signal-%lu, 32-blocked-%lu, 33-sigignore-%lu, 34-sigcatch-%lu, 35-wchan-%lu, 
	//	36-nswap-%lu, 37-cnswap-%lu, 38-exit_signal-%d, 39-processor-%d, 40-rt_priority-%u, 
	//	41-policy-%u, 42-delayacct_blkio_ticks-%llu, 43-guest_time-%lu, 44-cguest_time-%ld)
	if(3 != sscanf(content, 
		// warning: use of assignment suppression and length modifier together in gnu_scanf format [-Wformat]
		//"%*d %*s %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu %*ld %*ld %*ld %*ld %*ld %*ld %llu", 
		"%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %*d %*d %llu", 
		&utime, &stime, &starttime))
		return -(int)EINVAL;

	assert(sysconf(_SC_CLK_TCK) == HZ);
	system_uptime(&uptime);
	*createTime = (time64_t)uptime*1000 - starttime*1000/HZ; // jiffies -> millisecond
	*userTime = utime * 1000 / HZ;
	*kernelTime = stime * 1000 / HZ;
	return 0;
#endif
}

int process_memory_usage(pid_t pid, int *memKB, int *vmemKB)
{
#if defined(_WIN32)
	HANDLE handle;
	PROCESS_MEMORY_COUNTERS pmc;

	memset(&pmc, 0, sizeof(pmc));
	pmc.cb = sizeof(pmc);

	handle = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pid);
	if(!handle)
		return (int)GetLastError();

	if(!GetProcessMemoryInfo(handle, &pmc, sizeof(pmc)))
	{
		CloseHandle(handle);
		return (int)GetLastError();
	}
	CloseHandle(handle);

	*memKB = (int)(pmc.WorkingSetSize / 1024); // bytes -> KB
	*vmemKB = (int)(pmc.PagefileUsage / 1024);
	return 0;
#else
	int r;
	unsigned int size, resident;
	char content[2*1024] = {0};
	
	sprintf(content, "/proc/%d/statm", pid);
	r = tools_cat(content, content, sizeof(content));
	if(r < 0)
		return r;

	// linux: man proc
	if(2 != sscanf(content, "%u %u", &size, &resident))
		return -(int)EINVAL;

	// PAGE_SIZE = /proc/meminfo/Mapped / /proc/vmstat/nr_mapped
	*memKB = size * sysconf(_SC_PAGE_SIZE) / 1024; // bytes -> KB
	*vmemKB = resident * sysconf(_SC_PAGE_SIZE) / 1024;
	return 0;
#endif
}

#if defined(_WIN32) || defined(_WIN64)
int process_getmodules(pid_t pid, fcb_process_getmodules callback, void* param)
{
	int major, minor;
	DWORD i;
	DWORD ret;
	DWORD bytes;
	HANDLE handle;
	HMODULE modules[1024] = {0};
	CHAR filename[MAX_PATH] = {0};

	// open process
	handle = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pid);
	if(!handle)
		return (int)GetLastError();

	// enum process modules
	system_version(&major, &minor);
#if 0
	if(major >= 6)
		ret = EnumProcessModulesEx(handle, modules, sizeof(modules), &bytes, LIST_MODULES_ALL);
	else
#endif
		ret = EnumProcessModules(handle, modules, sizeof(modules), &bytes);

	if(!ret)
		return (int)GetLastError();

	// get module filename
	for(i=0; i<bytes/sizeof(HMODULE); i++)
	{
		if(GetModuleFileNameExA(handle, modules[i], filename, sizeof(filename)-1))
			callback(param, filename);
	}

	// close process
	CloseHandle(handle);
	return 0;
}
#else
int process_getmodules(pid_t pid, fcb_process_getmodules callback, void* param)
{
	char p[512];
	char module[PATH_MAX];
	FILE* fp;

	snprintf(p, sizeof(p), "/proc/%d/maps", pid);

	fp = fopen(p, "r");
	if(!fp)
		return -errno;

	while(fgets(p, sizeof(p)-1, fp))
	{
		if(1 == sscanf(p, "%*s %*s %*s %*s %*s %s", module))
			callback(param, module);
	}

	fclose(fp);
	return 0;
}
#endif

int process_getmodulename(const void *address, char *name, int len)
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
		return -errno;

	r = -1;
	while(fgets(p, sizeof(p)-1, fp))
	{
		if(3 != sscanf(p, "%p-%p %*s %*s %*s %*s %s", &addressfrom, &addressend, module))
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

#if defined(_WIN32) || defined(_WIN64)
#include <winternl.h>
typedef NTSTATUS (NTAPI *_NtQueryInformationProcess) (
	IN HANDLE ProcessHandle,
	IN PROCESSINFOCLASS ProcessInformationClass,
	OUT PVOID ProcessInformation,
	IN ULONG ProcessInformationLength,
	OUT PULONG ReturnLength OPTIONAL
);

int process_getcommandline(pid_t pid, char* cmdline, int bytes)
{
	HANDLE handle;
	NTSTATUS status;
	PVOID rtlUserProcParamsAddress;
	WCHAR *commandLineContents;
	UNICODE_STRING commandLine;
	PROCESS_BASIC_INFORMATION pbi;
	_NtQueryInformationProcess api;

	handle = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pid);
	if(!handle)
		return (int)GetLastError();

	api = (_NtQueryInformationProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
	if(0 == api(handle, ProcessBasicInformation, &pbi, sizeof(pbi), NULL))
	{
		if(ReadProcessMemory(handle, (PCHAR)pbi.PebBaseAddress + 0x10, &rtlUserProcParamsAddress, sizeof(PVOID), NULL))
		{
			if(ReadProcessMemory(handle, (PCHAR)rtlUserProcParamsAddress + 0x40, &commandLine, sizeof(commandLine), NULL))
			{
				commandLineContents = (WCHAR *)malloc(commandLine.Length);
				if(ReadProcessMemory(handle, commandLine.Buffer, commandLineContents, commandLine.Length, NULL))
				{
					WideCharToMultiByte(CP_ACP, 0, commandLineContents, commandLine.Length, cmdline, bytes/sizeof(wchar_t), NULL, NULL);
					status = 0;
				}
				free(commandLineContents);
			}
		}
	}

	CloseHandle(handle);
	return GetLastError();
}
#else
int process_getcommandline(pid_t pid, char* cmdline, int bytes)
{
	char file[128];
	char content[256];
	void *p, *pe;
	FILE *fp;
	int r, n, total, remain;

	sprintf(file, "/proc/%d/cmdline", pid);
	fp = fopen(file, "rb");
	if(!fp)
		return -errno;

	total = 0;
	remain = 0;
	r = fread(content, 1, sizeof(content), fp);
	while(r > 0)
	{
		r += remain;
		p =  content;
		do
		{
			pe = memchr(p, '\0', r);
			if(!pe)
				break;

			n = (char*)pe - (char*)p;
			if(total + 1 + n + 1 > bytes)
			{
				r = ENOBUFS;
				break;
			}

			if(total > 0)
				cmdline[total++] = ' '; // args seperator
			memcpy(cmdline+total, p, n);
			total += n; // string + blank
			cmdline[total] = '\0';

			p = (char*)pe + 1;
			assert((char*)p <= content + r);
		} while((char*)p < content+r);

		// move remain data
		assert(p != content); // len(argument) > sizeof(content)
		remain = r - ((char*)p- content);
		memmove(content, p, remain);

		// read more data
		r = fread(content+remain, 1, sizeof(content)-remain, fp);
	}

	fclose(fp);
	return r;
}
#endif
