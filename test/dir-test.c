#include "port/file-watcher.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "sys/path.h"

#define FILE_NAME	"file-watcher-testfile"
#define FILE_NAME2	"file-watcher-testfile2"

int dir_list(const char* path, int (onlist)(void* param, const char* name, int isdir), void* param);

static int dir_notify_onsubfile_create(void* param, void* file, int events, const char* name, int bytes)
{
	assert(strlen(FILE_NAME) == bytes && 0 == memcmp(FILE_NAME, name, bytes));
	*(int*)param = events;
	return 0;
}

static int dir_notify_onsubfile_rename(void* param, void* file, int events, const char* name, int bytes)
{
	assert(strlen(FILE_NAME2) == bytes && 0 == memcmp(FILE_NAME2, name, bytes));
	*(int*)param = events;
	return 0;
}

static int dir_notify_onsubfile_delete(void* param, void* file, int events, const char* name, int bytes)
{
	assert(strlen(FILE_NAME2) == bytes && 0 == memcmp(FILE_NAME2, name, bytes));
	*(int*)param = events;
	return 0;
}

static int dir_onlist(void* param, const char* name, int isdir)
{
	if (!isdir && 0 == strcmp(FILE_NAME, name))
		*(int*)param = 1;
	return 0;
}

void dir_test(void)
{
	int flags;
	void* wd;
	FILE* fp;
	file_watcher_t watcher;

	path_rmfile(FILE_NAME);

	watcher = file_watcher_create();
	wd = file_watcher_add(watcher, ".", FILE_WATCHER_EVENT_CREATE | FILE_WATCHER_EVENT_DELETE | FILE_WATCHER_EVENT_RENAME);
	assert(watcher && wd);

	fp = fopen(FILE_NAME, "wb");
	fclose(fp);

	flags = 0;
	assert(0 == dir_list(".", dir_onlist, &flags) && flags);
	flags = 0;
	assert(0 == file_watcher_process(watcher, 1000, dir_notify_onsubfile_create, &flags) && (FILE_WATCHER_EVENT_CREATE & flags));

	flags = 0;
	assert(0 == path_rename(FILE_NAME, FILE_NAME2) && 0 == file_watcher_process(watcher, 1000, dir_notify_onsubfile_rename, &flags) && (FILE_WATCHER_EVENT_RENAME & flags));

	flags = 0;
	assert(0 == path_rmfile(FILE_NAME2) && 0 == file_watcher_process(watcher, 1000, dir_notify_onsubfile_delete, &flags) && (FILE_WATCHER_EVENT_DELETE & flags));

	assert(0 == file_watcher_delete(watcher, wd));
	file_watcher_destroy(watcher);
}
