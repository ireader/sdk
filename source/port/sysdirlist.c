#include "sys/path.h"
#include <errno.h>

#if defined(OS_WINDOWS)
#include <Windows.h>
#else
#include <dirent.h>
#endif
#include <stdlib.h>
#include <string.h>

int dir_list(const char* path, int (onlist)(void* param, const char* name, int isdir), void* param)
{
	int r;
#if defined(OS_WINDOWS)
	BOOL next;
    HANDLE handle;
	WIN32_FIND_DATAA data;
	char pathext[MAX_PATH];

	// dir with wildchar
	r = snprintf(pathext, sizeof(pathext), "%s\\*", path);
	if (r >= sizeof(pathext) || r < 1)
		return -1;

    handle = FindFirstFileA(pathext, &data);
	if (handle == INVALID_HANDLE_VALUE)
		return -1;

	next = TRUE;
	for(r = 0; 0 == r && next; next = FindNextFileA(handle, &data))
	{
		if (0 == strcmp(data.cFileName, ".") || 0 == strcmp(data.cFileName, ".."))
			continue;

		r = onlist(param, data.cFileName, (data.dwFileAttributes& FILE_ATTRIBUTE_DIRECTORY)?1:0);
	}

	FindClose(handle);
    return r;
#else
	DIR* dir;
	struct dirent* p;

	dir = opendir(path);
	if (!dir)
		return -(int)errno;

	r = 0;
	for (p = readdir(dir); p && 0 == r; p = readdir(dir))
	{
		if (0 == strcmp(p->d_name, ".") || 0 == strcmp(p->d_name, ".."))
			continue;

		r = onlist(param, p->d_name, DT_DIR==p->d_type?1:0);
	}

	closedir(dir);
	return r;
#endif
}
