#ifndef _platform_pipe_h_
#define _platform_pipe_h_

#if defined(OS_WINDOWS)
#include <Windows.h>

typedef HANDLE pipe_t;
#define invalid_pipe_value INVALID_HANDLE_VALUE

#define PIPE_FLAG_READ	PIPE_ACCESS_INBOUND
#define PIPE_FLAG_WRITE	PIPE_ACCESS_OUTBOUND

#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define PIPE_FLAG_READ	O_RDONLY
#define PIPE_FLAG_WRITE	O_WRONLY

typedef int pipe_t;
#define invalid_pipe_value (-1)
#endif

//-------------------------------------------------------------------------------------
// pipe_t pipe_server(const char* name, int flags)
// pipe_t pipe_open(const char* name, int flags)
// int pipe_close(pipe_t pipe)
// int pipe_write(pipe_t pipe, const void* msg, int len)
// int pipe_read(pipe_t pipe, void* msg, int len)
//-------------------------------------------------------------------------------------

/// windows: pipe_server("\\.\pipe\pipename")
/// linux:	pipe_server("pipename")
/// Linux pipe can't be open with read and write
static inline pipe_t pipe_server(const char* name, int flags)
{
#if defined(OS_WINDOWS)
	HANDLE pipe;
	pipe = CreateNamedPipeA(name, flags, PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT, 1, 4096, 4096, 0, NULL);
	if(INVALID_HANDLE_VALUE == pipe)
		return invalid_pipe_value;

	if(ConnectNamedPipe(pipe, NULL) ? TRUE : GetLastError()==ERROR_PIPE_CONNECTED)
		return pipe;

	CloseHandle(pipe);
	return invalid_pipe_value;

#else
	int r;
	int pipe;
	r = mkfifo(name, O_CREAT|O_EXCL);
	if(r < 0 && errno != EEXIST)
		return invalid_pipe_value;

	pipe = open(name, flags, 0);
	if(-1 != pipe)
		return pipe;
	return invalid_pipe_value;
#endif
}

/// windows: pipe_server("\\.\pipe\pipename")
/// linux:	pipe_server("pipename")
/// Linux pipe can't be open with read and write
static inline pipe_t pipe_open(const char* name, int flags)
{
#if defined(OS_WINDOWS)
	HANDLE pipe;
	DWORD dwMode;
	DWORD access;

	access = 0;
	if(flags & PIPE_FLAG_READ)
		access |= GENERIC_READ;
	if(flags & PIPE_FLAG_WRITE)
		access |= GENERIC_WRITE;

	pipe = CreateFileA(name, access, 0, NULL, OPEN_EXISTING, 0, NULL);
	if(pipe == INVALID_HANDLE_VALUE)
		return invalid_pipe_value;

	dwMode = PIPE_READMODE_MESSAGE;
	if(SetNamedPipeHandleState(pipe, &dwMode, NULL, NULL))
		return pipe;

	CloseHandle(pipe);
	return invalid_pipe_value;
#else
	int pipe = open(name, flags, 0);
	if(-1 != pipe)
		return pipe;
	return invalid_pipe_value;
#endif
}

/// close pipe line
/// @param[in] pipe id
/// @return 0-ok, other-error
static inline int pipe_close(pipe_t pipe)
{
#if defined(OS_WINDOWS)
	CloseHandle(pipe);
#else
	close(pipe);
#endif
	return 0;
}

/// send message
/// @param[in] pipe id
/// @param[in] msg message to send
/// @param[in] len message length in byte
/// @return >0-send bytes, other-error
static inline int pipe_write(pipe_t pipe, const void* msg, int len)
{
#if defined(OS_WINDOWS)
	DWORD bytes = 0;
	return WriteFile(pipe, msg, len, &bytes, NULL) ? (int)bytes : -1*(int)GetLastError();
#else
	return write(pipe, msg, (size_t)len);
#endif
}

/// receive message
/// @param[in] pipe id
/// @param[out] msg message buffer
/// @param[in] len message length in byte
/// @return >0-receive bytes, other-error
static inline int pipe_read(pipe_t pipe, void* msg, int len)
{
#if defined(OS_WINDOWS)
	DWORD bytes = 0;
	return ReadFile(pipe, msg, len, &bytes, NULL) ? (int)bytes : -1*(int)GetLastError();
#else
	return read(pipe, msg, (size_t)len);
#endif
}

#endif /* !_platform_pipe_h_ */
