#if defined(OS_WINDOWS)
#include <Windows.h>
#include <Dbghelp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#pragma comment(lib, "Dbghelp.lib")

int system_backtrace(void* stacks[], int num, uint64_t* hash)
{
	int r;
	ULONG BackTraceHash;
	r = CaptureStackBackTrace(0, num, stacks, &BackTraceHash);
	*hash = BackTraceHash;
	return r;
}

typedef struct SYMBOL_INFO_t {
    SYMBOL_INFO symbol;
    char name[256];
} SYMBOL_INFO_t;

void system_symbol_init()
{
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
    SymSetOptions(SymGetOptions() | SYMOPT_DEFERRED_LOADS | SYMOPT_NO_IMAGE_SEARCH);
}

int system_symbol_resolve(void* address, char* line, int size)
{
    int n;
    DWORD64 offset;
    SYMBOL_INFO_t symbol;
    IMAGEHLP_LINE image;
    const char* basename;

    n = 0;
    offset = 0;
    memset(&symbol, 0, sizeof(symbol));
    symbol.symbol.SizeOfStruct = sizeof(symbol.symbol);
    symbol.symbol.MaxNameLen = sizeof(symbol.name);
    if (SymFromAddr(GetCurrentProcess(), (DWORD64)address, &offset, &symbol.symbol))
        n += snprintf(line + n, size - n, "%s+0x%x", symbol.symbol.Name, (unsigned int)offset);
    else
        n += snprintf(line + n, size - n, "unresolved symbol: 0x%p", address);

    offset = 0;
    memset(&image, 0, sizeof(image));
    image.SizeOfStruct = sizeof(image);
    if (n + 1 < size && n > 0 && SymGetLineFromAddr(GetCurrentProcess(), (DWORD64)address, (PDWORD)&offset, &image))
    {
        basename = strrchr(image.FileName ? image.FileName : "", '\\');
        basename = basename ? basename + 1 : "";
        n += snprintf(line + n, size - n, " [%s:%d]", basename, (int)image.LineNumber);
    }
	return n;
}

static int system_thread_stack_walk(HANDLE hProcess, HANDLE hThread, char* line, int size)
{
    int n;
    CONTEXT ctx;
    STACKFRAME frame;

    ZeroMemory(&ctx, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_ALL;
    if (!GetThreadContext(hThread, &ctx))
        return -1;

    ZeroMemory(&frame, sizeof(frame));
    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    n = 0;
    while (n < size && StackWalk(IMAGE_FILE_MACHINE_AMD64, hProcess, hThread, &frame, &ctx, NULL, SymFunctionTableAccess, SymGetModuleBase, NULL))
    {
        n += system_symbol_resolve((void*)frame.AddrPC.Offset, line + n, size - n);
        if (n + 1 < size)
            line[n++] = '\n';
    }

    return n;
}

int system_thread_backtrace(DWORD tid, char* line, int size)
{
    int n;
    HANDLE hThread;
    HANDLE hProcess;

    hProcess = GetCurrentProcess();
    if (GetCurrentThreadId() == tid)
        return system_thread_stack_walk(hProcess, GetCurrentThread(), line, size);

    hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, tid);
    if (hThread == INVALID_HANDLE_VALUE)
        return -1;

    if (SuspendThread(hThread) == (DWORD)-1)
    {
        CloseHandle(hThread);
        return -1;
    }
    
    n = system_thread_stack_walk(hProcess, hThread, line, size);

    ResumeThread(hThread);
    CloseHandle(hThread);
    return n;
}
#endif
