#ifndef _platform_mmap_h_
#define _platform_mmap_h_

#if defined(OS_WINDOWS)
#include <Windows.h>

#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <assert.h>

#ifndef IN
#define IN 
#endif

#ifndef OUT
#define OUT
#endif

enum mmap_mode{
	mmap_read			= 0x0001,
	mmap_write			= 0x0002,
	mmap_share			= 0x0100,
	mmap_create_always	= 0x1000,
	mmap_open_existing	= 0x2000,
};

typedef struct
{
	void*	ptr;
	size_t	len;

	// reserved
#if defined(OS_WINDOWS)
	HANDLE	_fd;
#else
	int		_fd;
#endif
} mmap_t;

//-------------------------------------------------------------------------------------
// int mmap_create(IN mmap_t* view, IN const char* filename, IN int flags, IN size_t offset, IN size_t length);
// int mmap_open(IN mmap_t* view, IN const char* filename, IN int flags, IN size_t offset, IN size_t length);
// int mmap_flush(IN mmap_t* view);
// void mmap_close(IN mmap_t* view);
//-------------------------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
///
/// flags: mmap_mode
/// 0-success, other-error
//////////////////////////////////////////////////////////////////////////
static inline int mmap_create(IN mmap_t* view, IN const char* filename, IN int flags, IN size_t offset, IN size_t length)
{
	void* p = NULL;

#if defined(OS_WINDOWS)
	HANDLE file;
	HANDLE mapping;
	DWORD share = 0;
	DWORD access = 0;
	DWORD create = 0;
	DWORD protect = 0;
	DWORD desire = 0;
	char* backslash = NULL;
	char name[MAX_PATH] = {0};
	
	// open file
	access |= (flags&mmap_read) ? GENERIC_READ : 0;
	access |= (flags&mmap_write) ? GENERIC_WRITE : 0;
	if(flags & mmap_share)
	{
		share |= (flags&mmap_read) ? FILE_SHARE_READ : 0;
		share |= (flags&mmap_write) ? FILE_SHARE_WRITE : 0;
	}
	create |= (flags&mmap_create_always) ? CREATE_ALWAYS : 0;
	create |= (flags&mmap_open_existing) ? OPEN_EXISTING : 0;

	file = CreateFileA(filename, access, share, NULL, create, FILE_ATTRIBUTE_NORMAL, NULL);
	if(INVALID_HANDLE_VALUE == file)
		return GetLastError();

	// create file view
	if(flags & mmap_write)
		protect = PAGE_READWRITE;
	else if(flags & mmap_read)
		protect = PAGE_READONLY;
	else
		return -1; // param error

	// replace '\\' -> '+'
	strcpy(name, filename);
	backslash = strchr(name, '\\');
	while(backslash)
	{
		*backslash = '+';
		backslash = strchr(backslash+1, '\\');
	}
	mapping = CreateFileMappingA(file, NULL, protect, 0, length, name);
	if(NULL == mapping)
		return GetLastError();

	// open view
	if(flags & mmap_write)
		desire |= FILE_MAP_WRITE;
	if(flags & mmap_read)
		desire |= FILE_MAP_READ;

	// map view
	p = MapViewOfFile(mapping, desire, 0, offset, length);
	if(NULL == p)
		return GetLastError();

	view->ptr = p;
	view->len = length;
	view->_fd = mapping;
	return 0;
#else
	char c;
	int fd;
	int share = 0;
	int access = O_RDONLY;
	int prot = PROT_NONE;
	
	assert(length>0 && 0 == offset % getpagesize());
	if(flags & mmap_write)
		access |= O_RDWR;
	else if(flags & mmap_read)
		access |= O_RDONLY;

	access |= (flags&mmap_create_always) ? O_CREAT : 0;
	fd = open(filename, access, 00777);
	if(-1 == fd)
		return errno;

	// increased to match the specified size
	if((off_t)-1 != lseek(fd, offset+length-1, SEEK_SET))
	{
		if(sizeof(c) != read(fd, &c, sizeof(c)))
		{
			if(sizeof(c) != write(fd, &c, sizeof(c)))
				return errno;
		}
	}

	// map view
	prot |= (flags&mmap_read) ? PROT_READ : 0;
	prot |= (flags&mmap_write) ? PROT_WRITE : 0;

	share = (flags & mmap_share) ? MAP_SHARED : MAP_PRIVATE;
	p = mmap(NULL, length, prot, share, fd, offset);
	if(0 == p)
		return errno;

	view->ptr = p;
	view->len = length;
	view->_fd = fd;
	return 0;
#endif
}

/// 0-success, other-error
static inline int mmap_open(IN mmap_t* view, IN const char* filename, IN int flags, IN size_t offset, IN size_t length)
{
#if defined(OS_WINDOWS)
	HANDLE mapping;
	DWORD desire = 0;
	char* backslash = NULL;
	char name[MAX_PATH] = {0};

	// open view
	if(flags & mmap_write)
		desire |= FILE_MAP_WRITE;
	if(flags & mmap_read)
		desire |= FILE_MAP_READ;
	
	// replace '\\' -> '+'
	strcpy(name, filename);
	backslash = strchr(name, '\\');
	while(backslash)
	{
		*backslash = '+';
		backslash = strchr(backslash+1, '\\');
	}
	mapping = OpenFileMappingA(desire, FALSE, filename);
	if(NULL == view)
		return GetLastError();

	// map view
	LPVOID p = MapViewOfFile(mapping, desire, 0, offset, length);
	if(NULL == p)
		return GetLastError();

	view->ptr = p;
	view->len = length;
	view->_fd = mapping;
	return 0;
#else
	return mmap_create(view, filename, flags, offset, length);
#endif
}

/// 0-success, other-error
static inline int mmap_flush(IN mmap_t* view)
{
#if defined(OS_WINDOWS)
	BOOL r = FlushViewOfFile(view->ptr, view->len);
	return TRUE==r?0:GetLastError();
#else
	return msync(view->ptr, view->len, MS_SYNC);
#endif
}

static inline void mmap_close(IN mmap_t* view)
{
#if defined(OS_WINDOWS)
	UnmapViewOfFile(view->ptr);
	CloseHandle(view->_fd);
#else
	munmap(view->ptr, view->len);
	close(view->_fd);
#endif
}

/*
/// 0-success, other-error
static inline int mmap_open_ex(OUT mmap_t* view, IN const char* filename, IN size_t offset, IN size_t size)
{
	int r = 0;
	int flags = mmap_read|mmap_write|mmap_share|mmap_open_existing;
	r = mmap_open(view, filename, flags, offset, size);
	if(0 != r)
	{
		// open existing
		r = mmap_create(view, filename, flags, offset, size);
		if(0 != r)
		{
			if(ENOENT == r)
			{
				// create new
				r = mmap_create(view, filename, mmap_read|mmap_write|mmap_share|mmap_create_always, offset, size);
			}

			// try again
			if(0 != r)
			{
				r = mmap_open(view, filename, flags, offset, size);
			}
		}
	}
	return r;
}
*/

#endif /* !_platform_mmap_h_ */
