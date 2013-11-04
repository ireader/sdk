#ifndef _task_queue_h_
#define _task_queue_h_

#include "thread-pool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* task_queue_t;
typedef void (*task_proc)(void* param);

task_queue_t task_queue_create(thread_pool_t pool, int maxWorker);
int task_queue_destroy(task_queue_t taskQ);

int task_queue_post(task_queue_t taskQ, task_proc proc, void* param);

#ifdef __cplusplus
}
#endif

#endif /* !_task_queue_h_ */
