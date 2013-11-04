#ifndef _sys_task_queue_h_
#define _sys_task_queue_h_

#include "libsys.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* sys_task_queue_t;
typedef void (*sys_task_proc)(void* param);

LIBSYS_API sys_task_queue_t sys_task_queue_create(int maxWorker);
LIBSYS_API int sys_task_queue_destroy(sys_task_queue_t taskQ);
LIBSYS_API int sys_task_queue_post(sys_task_queue_t taskQ, sys_task_proc proc, void* param);

#ifdef __cplusplus
}
#endif

#endif /* !_sys_task_queue_h_ */
