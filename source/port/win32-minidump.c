#if defined(OS_WINDOWS)
#include <Windows.h>
#include <minidumpapiset.h>

#pragma comment(lib, "Dbghelp.lib")

#define TINYDUMP MiniDumpNormal
#define MINIDUMP (MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory)
#define MIDIDUMP (MiniDumpWithPrivateReadWriteMemory | MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules)
#define MAXIDUMP (MiniDumpWithFullMemory | MiniDumpWithFullMemoryInfo | MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules)

LONG WINAPI CreateMiniDump(const char* filename, struct _EXCEPTION_POINTERS* pep)
{
    HANDLE hFile;
    MINIDUMP_TYPE flags;
    MINIDUMP_EXCEPTION_INFORMATION info;
    
    hFile = CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return EXCEPTION_CONTINUE_SEARCH;

    flags = MIDIDUMP;
    memset(&info, 0, sizeof(info));
    info.ThreadId = GetCurrentThreadId();
    info.ExceptionPointers = pep;
    info.ClientPointers = FALSE;
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, flags, &info, 0, 0);

    CloseHandle(hFile);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif