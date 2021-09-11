#include <Windows.h>
#include <minidumpapiset.h>

#pragma comment(lib, "Dbghelp.lib")

LONG WINAPI CreateMiniDump(struct _EXCEPTION_POINTERS* pep)
{
    HANDLE hFile;
    MINIDUMP_EXCEPTION_INFORMATION info;
    
    hFile = CreateFileA("core.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return EXCEPTION_CONTINUE_SEARCH;

    memset(&info, 0, sizeof(info));
    info.ThreadId = GetCurrentThreadId();
    info.ExceptionPointers = pep;
    info.ClientPointers = FALSE;
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal|MiniDumpWithThreadInfo, &info, 0, 0);

    CloseHandle(hFile);
    return EXCEPTION_EXECUTE_HANDLER;
}
