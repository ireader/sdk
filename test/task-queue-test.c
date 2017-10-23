#include "cstringext.h"
#include "sys/thread.h"
#include "sys/semaphore.h"
#include "task-queue.h"
#include "thread-pool.h"
#include <stdio.h>

#define N_TASK 1000

static semaphore_t s_sema;

static void tasktest(void* param)
{
	semaphore_post(&s_sema);
}

void task_queue_test(void)
{
	int i;
	thread_pool_t pool;
	task_queue_t taskQ;
	
	semaphore_create(&s_sema, NULL, 0);
	pool = thread_pool_create(8, 8, 8);
	taskQ = task_queue_create(pool, 8);
	for (i=0; i < N_TASK; ++i)
	{
		task_queue_post(taskQ, tasktest, NULL);
	}

	for(i = 0; i < N_TASK; ++i)
		semaphore_wait(&s_sema);

	task_queue_destroy(taskQ);
	thread_pool_destroy(pool);
	semaphore_destroy(&s_sema);

	printf("task-queue test ok\n");
}
