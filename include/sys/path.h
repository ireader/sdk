#ifndef _platform_path_h_
#define _platform_path_h_

#if defined(OS_WINDOWS)
#include <Windows.h>
#include <direct.h>

#define PATH_MAX _MAX_PATH

#else
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
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

static inline int path_dirname(const char* fullname)
{
	const char* p = path_basename(fullname);
	if(p == fullname)
		return 0; // don't valid path name
	return p - 1 - fullname; // skip /
}

/// path_concat("a/./b/../c.txt", "e:\dir\") => e:\dir\a\c.txt
/// path_concat("../c.txt", "e:\dir\") => error
/// @return 0-ok, other-error
static inline int path_concat(const char* path, const char* localdir, char fullpath[PATH_MAX])
{
    int r;
    char userpath[PATH_MAX];
    r = snprintf(userpath, PATH_MAX - 1, "%s%s", localdir, path);
    r = (r > 0 && r < sizeof(userpath)-1) ? path_realpath(userpath, fullpath) : -1;
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

static inline int path_isabsolute(const char* path, size_t n)
{
    size_t off;
    if (n < 1)
        return 0;

    if ('/' == path[0])
        return 1;

#if defined(OS_WINDOWS)
    // 1. \ (relative to current working directory root)
    // 2. [driver_letter]:\
    // 3. \\[server]\]sharename]\
    // 4. \\?\[driver_spec]:\
    // 5. \\?\[server]\[sharename]\
    // 6. \\?\UNC\[server]\[sharename]\
    // 7. \\.\[physical_device]\  e.g. \\.\COM1\ 

    if (n > 3)
    {
        if (':' == path[1] && '\\' == path[2] && (('a' <= path[0] && 'z' >= path[0]) || ('A' <= path[0] && 'Z' >= path[0])))
            return 1; // case 2
        else if ('\\' == path[0] && '\\' == path[1] && '\\' != path[3])
            return 1; // case 3/4/5/6/7
        else if ('\\' == path[0] && '\\' != path[1])
            return 1; // case 1
    }
#endif

#if 1
    // url schema
    off = strcspn(path, ":/\\");
    if (off + 2 < n && ':' == path[off] && '/' == path[off + 1] && '/' == path[off + 2])
        return 1; // e.g. http://
#endif

    return 0;
}

/// path_resolve("abc.txt", "/path/to/dir") ==> /path/to/dir/abc.txt
/// @return 0-ok with non-absolute directory, 1-ok with absolute directory, <0-error
static inline int path_resolve(char* buf, size_t len, const char* path, const char* base)
{
    int r;
    size_t n, m;

    n = path ? strlen(path) : 0;
    m = base ? strlen(base) : 0;
    r = path_isabsolute(path, n);
    if (m > 0 && 0 == r)
    {
        r = path_isabsolute(base, m);
        while (m > 0 && ('/' == base[m - 1] || '\\' == base[m - 1]))
            m--; // filter last /

        if (m + n + 1 /* '/' */ + 1 /* '\0' */ > len)
            return -1;

        memmove(buf + m + 1, path, n + 1); // with '\0'
        memmove(buf, base, m);
        buf[m] = '/'; // TODO: windows \ separator
        return r;
    }
    else
    {
        if (n + 1 > len)
            return -1;

        //path_resolve2 memory safe: snprintf(buf, len, "%s", path);
        memmove(buf, path, n + 1); // with '\0'
        return r;
    }
}

/// path_resolve2("abc.txt", "path/to/dir", "/root/") ==> /root/path/to/dir/abc.txt
/// path_resolve2("abc.txt", "/root1", "/root2/") ==> /root1/abc.txt
/// @return 0-ok with non-absolute directory, 1-ok with absolute directory, <0-error
static inline int path_resolve2(char* buf, size_t len, const char* path, const char* base1, ...)
{
    int r;
    va_list va;
    r = path_resolve(buf, len, path, base1);

    va_start(va, base1);
    while (0 == r)
    {
        base1 = va_arg(va, const char*);
        r = path_resolve(buf, len, buf, base1); // memory safe with buf
    }
    va_end(va);

    return r;
}

#endif /* !_platform_path_h_ */
