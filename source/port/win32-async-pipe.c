#if defined(OS_WINDOWS)
#include "port/async-pipe.h"
#include <assert.h>
#include <Windows.h>

typedef struct  
{
	OVERLAPPED overlap;
	HANDLE pipe;

	void* param;
	async_pipe_onread read;
	async_pipe_onwrite write;

#if defined(_DEBUG)
	long ref;
#endif
} Win32Pipe;

// pipe name: \\.\pipe\pipename
async_pipe_t async_pipe_server(const char* name)
{
	DWORD dwWait;
	Win32Pipe* o;

	o = (Win32Pipe*)GlobalAlloc(GPTR, sizeof(Win32Pipe));
	if(!o)
		return NULL;

	o->pipe = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, NULL);
	if(INVALID_HANDLE_VALUE != o->pipe)
	{
		o->overlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if(NULL != o->overlap.hEvent)
		{
			// MSDN: Overlapped ConnectNamedPipe should return zero. 
			if(0==ConnectNamedPipe(o->pipe, &o->overlap))
			{
				switch(GetLastError())
				{
				case ERROR_PIPE_CONNECTED:
					return o;

				case ERROR_IO_PENDING:
					do
					{
						dwWait = WaitForSingleObjectEx(o->overlap.hEvent, INFINITE, TRUE);
						if(WAIT_OBJECT_0 == dwWait)
						{
							if(GetOverlappedResult(o->pipe, &o->overlap, &dwWait, FALSE))
								return o;
						}
					} while(dwWait==WAIT_IO_COMPLETION);
					break;
				}
			}
			
			CloseHandle(o->overlap.hEvent);
		}
		CloseHandle(o->pipe);
	}

	GlobalFree(o);
	return NULL;
}

// pipe name: \\.\pipe\pipename
async_pipe_t async_pipe_open(const char* name)
{
	DWORD dwMode;
	Win32Pipe* o;

	o = (Win32Pipe*)GlobalAlloc(GPTR, sizeof(Win32Pipe));
	if(!o)
		return NULL;

	o->pipe = CreateFileA(name, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if(o->pipe != INVALID_HANDLE_VALUE)
	{
		dwMode = PIPE_READMODE_BYTE;
		if(SetNamedPipeHandleState(o->pipe, &dwMode, NULL, NULL))
			return o;

		CloseHandle(o->pipe);
	}
	GlobalFree(o);
	return NULL;
}

int async_pipe_close(async_pipe_t pipe)
{
	int err;

	Win32Pipe* o = (Win32Pipe*)pipe;
	
	// cancel ip
	CancelIo(o->pipe);

	// close pipe object
	assert(0 == o->ref);
	CloseHandle(o->pipe);
	err = (int)GetLastError();

	// close event object
	if(o->overlap.hEvent)
		CloseHandle(o->overlap.hEvent);

	// free
	GlobalFree(o);
	return -err;
}

static VOID WINAPI winpipe_onwrite(DWORD code, DWORD bytes, LPOVERLAPPED lpOverlapped)
{
	Win32Pipe* o = (Win32Pipe*)lpOverlapped;
	assert(0==InterlockedDecrement(&(o->ref)));
	o->write(o->param, code, bytes);
}

static VOID WINAPI winpipe_onread(DWORD code, DWORD bytes, LPOVERLAPPED lpOverlapped)
{
	Win32Pipe* o = (Win32Pipe*)lpOverlapped;
	assert(0==InterlockedDecrement(&(o->ref)));
	o->read(o->param, code, bytes);
}

int async_pipe_write(async_pipe_t pipe, const void* msg, int len, async_pipe_onwrite callback, void* param)
{
	Win32Pipe* o = (Win32Pipe*)pipe;
	o->write = callback;
	o->param = param;
	
	assert(1==InterlockedIncrement(&(o->ref)));
	if(!WriteFileEx(o->pipe, msg, len, &(o->overlap), winpipe_onwrite))
	{
		assert(0==InterlockedDecrement(&(o->ref)));
		return (int)GetLastError();
	}
	return 0;
}

int async_pipe_read(async_pipe_t pipe, void* msg, int len, async_pipe_onread callback, void* param)
{
	Win32Pipe* o = (Win32Pipe*)pipe;
	o->read = callback;
	o->param = param;

	assert(1==InterlockedIncrement(&(o->ref)));
	if(!ReadFileEx(o->pipe, msg, len, &(o->overlap), winpipe_onread))
	{
		assert(0==InterlockedDecrement(&(o->ref)));
		return (int)GetLastError();
	}
	return 0;
}

#endif