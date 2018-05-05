#ifndef _platform_path_h_
#define _platform_path_h_

#if defined(OS_WINDOWS)
#include <Windows.h>
#include <direct.h>

#define PATH_MAX _MAX_PATH

#else
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>

/// test file
/// 0-don't found, other-exist
static inline int path_testfile(const char* filename)
{
#if defined(OS_WINDOWS)
	// we must use GetFileAttributes() instead of the ANSI C functions because
	// it can cope with network (UNC) paths unlike them
	size_t ret = GetFileAttributesA(filename);
	return ((ret != (size_t)-1) && !(ret & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
#else
	struct stat info;
	return (stat(filename, &info)==0 && (info.st_mode&S_IFREG)) ? 1 : 0;
#endif
}

/// test file
/// 0-don't found, other-exist
static inline int path_testdir(const char* path)
{
#if defined(OS_WINDOWS)
	size_t ret = GetFileAttributesA(path);
	return ((ret != (size_t)-1) && (ret & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
#else
	struct stat info;
	return (stat(path, &info)==0 && (info.st_mode&S_IFDIR)) ? 1 : 0;
#endif
}

/// create a directory
/// 0-ok, other-error
static inline int path_makedir(const char* path)
{
#if defined(OS_WINDOWS)
	BOOL r = CreateDirectoryA(path, NULL);
	return TRUE==r ? 0 : (int)GetLastError();
#else
	int r = mkdir(path, 0777);
	return 0 == r ? 0 : errno;
#endif
}

/// create a directory
/// 0-ok, other-error
static inline int path_rmdir(const char* path)
{
#if defined(OS_WINDOWS)
	BOOL r = RemoveDirectoryA(path);
	return TRUE==r?0:(int)GetLastError();
#else
	int r = rmdir(path);
	return 0 == r? 0 : errno;
#endif
}

/// get current work directory
/// 0-ok, other-error
static inline int path_getcwd(char* path, unsigned int bytes)
{
#if defined(OS_WINDOWS)
	DWORD r = GetCurrentDirectoryA(bytes, path);
	return 0==r ? GetLastError() : 0;
#else
	char* p = getcwd(path, bytes);
	return p ? 0 : errno;
#endif
}

/// set work directory
/// 0-ok, other-error
static inline int path_chcwd(const char* path)
{
#if defined(OS_WINDOWS)
	BOOL r = SetCurrentDirectoryA(path);
	return TRUE==r ? 0 : (int)GetLastError();
#else
	int r = chdir(path);
	return 0 == r? 0 : errno;
#endif
}

/// get full path name
/// 0-ok, other-error
static inline int path_realpath(const char* path, char resolved_path[PATH_MAX])
{
#if defined(OS_WINDOWS)
	DWORD r = GetFullPathNameA(path, PATH_MAX, resolved_path, NULL);
	return 0==r ? 0 : (int)GetLastError();
#else
	char* p = realpath(path, resolved_path);
	return p ? 0 : errno;
#endif
}

static inline const char* path_basename(const char* fullname)
{
	const char* p = strrchr(fullname, '/');
	const char* p2 = strrchr(fullname, '\\');
	if(p2 > p) p = p2;
	return p ? p+1 : fullname;
}

static inline int path_dirname(const char* fullname, char* dir)
{
	const char* p = path_basename(fullname);
	if(p == fullname)
		return -1; // don't valid path name

	memmove(dir, fullname, p-fullname);
	dir[p-fullname - (p-fullname>1? 1 : 0)] = 0;
	return 0;
}

/// path_concat("a/./b/../c.txt", "e:\dir\") => e:\dir\a\c.txt
/// path_concat("../c.txt", "e:\dir\") => error
/// @return 0-ok, other-error
static inline int path_concat(const char* path, const char* localdir, char fullpath[PATH_MAX])
{
    int r;
    char userpath[PATH_MAX];
    snprintf(userpath, PATH_MAX - 1, "%s%s", localdir, path);
    r = path_realpath(userpath, fullpath);
	r = 0 != r ? r : path_realpath(localdir, userpath);
#if defined(OS_WINDOWS)
    return 0 != r ? r : _strnicmp(userpath, fullpath, strlen(userpath));
#else
    return 0 != r ? r : strncmp(userpath, fullpath, strlen(userpath));
#endif
}

/// delete a name and possibly the file it refers to
/// 0-ok, other-error
static inline int path_rmfile(const char* file)
{
#if defined(OS_WINDOWS)
	BOOL r = DeleteFileA(file);
	return TRUE==r ? 0 : (int)GetLastError();
#else
	int r = remove(file);
	return 0 == r? 0 : errno;
#endif
}

/// change the name or location of a file
/// 0-ok, other-error
static inline int path_rename(const char* oldpath, const char* newpath)
{
#if defined(OS_WINDOWS)
	BOOL r = MoveFileA(oldpath, newpath);
	return TRUE==r?0:(int)GetLastError();
#else
	int r = rename(oldpath, newpath);
	return 0 == r? 0 : errno;
#endif
}

/// get file size in bytes
/// return file size
static inline int64_t path_filesize(const char* filename)
{
#if defined(OS_WINDOWS)
	struct _stat64 st;
	if (0 == _stat64(filename, &st) && (st.st_mode & S_IFREG))
		return st.st_size;
	return -1;
#else
	struct stat st;
	if (0 == stat(filename, &st) && (st.st_mode & S_IFREG))
		return st.st_size;
	return -1;
#endif
}

#endif /* !_platform_path_h_ */
