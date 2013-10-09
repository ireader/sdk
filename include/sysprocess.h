#ifndef _sysprocess_h_
#define _sysprocess_h_

#include "time64.h"
#include "sys/process.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*fcb_process_list)(void* param, const char* name, process_t pid);
typedef void (*fcb_process_getmodules)(void* param, const char* module);

int process_kill_all(const char* name);

int process_list(fcb_process_list callback, void* param);

int process_time(process_t pid, time64_t* createTime, time64_t* kernelTime, time64_t* userTime);

int process_memory_usage(process_t pid, int *memKB, int *vmemKB);

int process_getmodules(process_t pid, fcb_process_getmodules callback, void* param);

int process_getmodulename(const void *address, char *name, int len);

int process_getcommandline(process_t pid, char* cmdline, int len);

#ifdef __cplusplus
}
#endif

#endif /* !_sysprocess_h_ */
