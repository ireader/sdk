#ifndef _task_queue_h_
#define _task_queue_h_

#include "thread-pool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int task_t;
typedef void (*task_proc)(task_t id, void* param);

int task_queue_create(thread_pool_t pool, int maxWorker);
int task_queue_destroy();

int task_queue_post(task_t id, task_proc proc, void* param);

#ifdef __cplusplus
}
#endif

#endif /* !_task_queue_h_ */
