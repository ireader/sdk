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
    IMAGEHLP_LINE64 image;

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
    if (n + 1 < size && n > 0 && SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)address, (PDWORD)&offset, &image))
        n += snprintf(line + n, size - n, " (%s:%d)", image.FileName, (int)image.LineNumber);

	return n;
}
