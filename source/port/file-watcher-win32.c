// https://blog.csdn.net/wzsy/article/details/6697613
#if defined(OS_WINDOWS)
#include "port/file-watcher.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <Windows.h>

#define MAX_FILE_WATCHER 128

struct file_watcher_win32_t;
struct file_watcher_item_t
{
	struct file_watcher_win32_t* watcher;

	HANDLE file;
//	HANDLE notify;
	OVERLAPPED overlapped;
	int events;
};

struct file_watcher_win32_t
{
	struct file_watcher_item_t* files;
	int cap;
	int num;

	char ptr[1024];

	file_watcher_notify onnotify;
	void* param;
};

static inline int events_to_windows(int events)
{
	int filters = 0;
	assert(0 == (events & (FILE_WATCHER_EVENT_OPEN | FILE_WATCHER_EVENT_CLOSE)));
	filters |= (events & FILE_WATCHER_EVENT_READ) ? FILE_NOTIFY_CHANGE_LAST_ACCESS : 0;
	filters |= (events & FILE_WATCHER_EVENT_WRITE) ? FILE_NOTIFY_CHANGE_LAST_WRITE : 0; // FILE_NOTIFY_CHANGE_SIZE
	filters |= (events & FILE_WATCHER_EVENT_CHMOD) ? FILE_NOTIFY_CHANGE_ATTRIBUTES : 0;
	filters |= (events & FILE_WATCHER_EVENT_CREATE) ? FILE_NOTIFY_CHANGE_CREATION : 0;
	filters |= (events & FILE_WATCHER_EVENT_DELETE) ? (FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME) : 0;
	filters |= (events & FILE_WATCHER_EVENT_RENAME) ? (FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME) : 0;
	return filters;
}

static VOID WINAPI file_watcher_routine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
	int r;
	char filename[MAX_PATH] = { 0 };
	FILE_NOTIFY_INFORMATION* info;
	struct file_watcher_item_t* file;
	struct file_watcher_win32_t* watcher;
	file = (struct file_watcher_item_t*)lpOverlapped->hEvent;
	watcher = (struct file_watcher_win32_t*)file->watcher;

	info = (FILE_NOTIFY_INFORMATION*)watcher->ptr;
	switch (info->Action)
	{
	case FILE_ACTION_ADDED:
		if (file->events & FILE_WATCHER_EVENT_CREATE)
		{
			r = WideCharToMultiByte(CP_UTF8, 0, info->FileName, info->FileNameLength / 2, filename, sizeof(filename), NULL, NULL);
			r = watcher->onnotify(watcher->param, file, FILE_WATCHER_EVENT_CREATE, filename, r);
		}
		break;
	case FILE_ACTION_REMOVED:
		if (file->events & FILE_WATCHER_EVENT_DELETE)
		{
			r = WideCharToMultiByte(CP_UTF8, 0, info->FileName, info->FileNameLength / 2, filename, sizeof(filename), NULL, NULL);
			r = watcher->onnotify(watcher->param, file, FILE_WATCHER_EVENT_DELETE, filename, r);
		}
		break;
	case FILE_ACTION_MODIFIED:
		if (file->events & FILE_WATCHER_EVENT_CREATE)
		{
			r = WideCharToMultiByte(CP_UTF8, 0, info->FileName, info->FileNameLength / 2, filename, sizeof(filename), NULL, NULL);
			r = watcher->onnotify(watcher->param, file, FILE_WATCHER_EVENT_WRITE, filename, r);
		}
		break;
	case FILE_ACTION_RENAMED_NEW_NAME:
		if (file->events & FILE_WATCHER_EVENT_RENAME)
		{
			r = WideCharToMultiByte(CP_UTF8, 0, info->FileName, info->FileNameLength / 2, filename, sizeof(filename), NULL, NULL);
			//r = watcher->onnotify(watcher->param, file, FILE_WATCHER_EVENT_RENAME, filename, r);
		}
		break;
	case FILE_ACTION_RENAMED_OLD_NAME:
		if (file->events & FILE_WATCHER_EVENT_RENAME)
		{
			if (info->NextEntryOffset)
				info = (FILE_NOTIFY_INFORMATION*)(watcher->ptr + info->NextEntryOffset);
			r = WideCharToMultiByte(CP_UTF8, 0, info->FileName, info->FileNameLength/2, filename, sizeof(filename), NULL, NULL);
			r = watcher->onnotify(watcher->param, file, FILE_WATCHER_EVENT_RENAME, filename, r);
		}
		break;
	default:
		break;
	}

	// next round
	if (file->events)
	{
		memset(&file->overlapped, 0, sizeof(file->overlapped));
		file->overlapped.hEvent = (HANDLE)file;
		if (!ReadDirectoryChangesW(file->file, watcher->ptr, sizeof(watcher->ptr), TRUE, events_to_windows(file->events), &r, &file->overlapped, file_watcher_routine))
		{
			assert(0);
		}
	}
}

file_watcher_t file_watcher_create(void)
{
	struct file_watcher_win32_t* watcher;
	watcher = calloc(1, sizeof(*watcher));
	return watcher;
}

void file_watcher_destroy(file_watcher_t p)
{
	struct file_watcher_win32_t* watcher;
	watcher = (struct file_watcher_win32_t*)p;
	
	if (watcher)
	{
		if (watcher->files && watcher->cap > 0)
		{
			watcher->cap = 0;
			watcher->num = 0;
			free(watcher->files);
		}

		free(watcher);
	}
}

void* file_watcher_add(file_watcher_t p, const char* fileordir, int events)
{
	int filters;
	DWORD bytes;
	struct file_watcher_item_t* file;
	struct file_watcher_win32_t* watcher;
	watcher = (struct file_watcher_win32_t*)p;
	if (!watcher)
		return NULL;

	assert(watcher->num < MAX_FILE_WATCHER);
	if (watcher->num + 1 >= watcher->cap)
	{
		file = (struct file_watcher_item_t*)realloc(watcher->files, sizeof(struct file_watcher_item_t) * (watcher->cap + 4));
		if (!file)
			return NULL;
		watcher->files = file;
		watcher->cap += 4;
	}

	file = watcher->files + watcher->num;
	file->file = CreateFileA(fileordir, FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, NULL);
	if (INVALID_HANDLE_VALUE == file->file)
		return NULL;

	file->watcher = watcher;
	file->events = events;
	filters = events_to_windows(events);
	//file->notify = FindFirstChangeNotificationA(fileordir, TRUE, filters);
	//if (INVALID_HANDLE_VALUE == file->notify)
	//{
	//	CloseHandle(file->file);
	//	return NULL;
	//}

	watcher->num++;

	memset(&file->overlapped, 0, sizeof(file->overlapped));
	file->overlapped.hEvent = (HANDLE)file;
	if (!ReadDirectoryChangesW(file->file, watcher->ptr, sizeof(watcher->ptr), TRUE, events_to_windows(file->events), &bytes, &file->overlapped, file_watcher_routine))
	{
		file_watcher_delete(watcher, file);
		return NULL;
	}
	return file;
}

int file_watcher_delete(file_watcher_t p, void* file)
{
	int i;
	struct file_watcher_win32_t* watcher;
	watcher = (struct file_watcher_win32_t*)p;
	if (!watcher || !file || INVALID_HANDLE_VALUE == file)
		return -EINVAL;

	for (i = 0; i < watcher->num; i++)
	{
		if (watcher->files+i == file)
		{
			//if (watcher->files[i].notify && INVALID_HANDLE_VALUE != watcher->files[i].notify)
			//	FindCloseChangeNotification(watcher->files[i].notify);
			if (watcher->files[i].file && INVALID_HANDLE_VALUE != watcher->files[i].file)
			{
				watcher->files[i].events = 0; // stop
				memset(&watcher->files[i].overlapped, 0, sizeof(watcher->files[i].overlapped));
				watcher->files[i].overlapped.hEvent = (HANDLE)(watcher->files + i);
				CancelIoEx(watcher->files[i].file, &watcher->files[i].overlapped);
				WaitForSingleObjectEx(watcher->files[i].file, 0, TRUE);
				CloseHandle(watcher->files[i].file);
			}

			if(i + 1 < watcher->num)
				memmove(watcher->files + i, watcher->files + i + 1, sizeof(struct file_watcher_item_t)*(watcher->num - i - 1));
			watcher->num--;
			return 0;
		}
	}

	return -ENOENT;
}

int file_watcher_process(file_watcher_t p, int timeout, file_watcher_notify onnotify, void* param)
{
	int i;
	DWORD r;
	HANDLE handlers[MAX_FILE_WATCHER];
	struct file_watcher_item_t* file;
	struct file_watcher_win32_t* watcher;
	watcher = (struct file_watcher_win32_t*)p;
	if (!watcher)
		return -EINVAL;

	watcher->param = param;
	watcher->onnotify = onnotify;
	for (i = 0; i < watcher->num; i++)
	{
		handlers[i] = watcher->files[i].file;
	}

	r = WaitForMultipleObjectsEx(watcher->num, handlers, FALSE, timeout, TRUE);
	if (WAIT_OBJECT_0 <= r && r < WAIT_OBJECT_0 + watcher->num)
	{
		assert(0);
	}
	else if (WAIT_IO_COMPLETION == r)
	{
		// APC done
		return 0;
	}

	return WAIT_TIMEOUT == r ? -ETIMEDOUT : -1;
}

#endif