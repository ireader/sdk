#include "sys-task-queue.h"
#include "task-queue.h"

extern thread_pool_t g_pool;

sys_task_queue_t sys_task_queue_create(int maxWorker)
{
	return task_queue_create(g_pool, maxWorker);
}

int sys_task_queue_destroy(sys_task_queue_t taskQ)
{
	return task_queue_destroy(taskQ);
}

int sys_task_queue_post(sys_task_queue_t taskQ, sys_task_proc proc, void* param)
{
	return task_queue_post(taskQ, proc, param);
}
