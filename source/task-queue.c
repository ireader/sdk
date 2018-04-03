#include "task-queue.h"
#include "sys/atomic.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/locker.h"
#include "sys/sema.h"
#include "thread-pool.h"
#include "list.h"
#include <errno.h>

enum { PRIORITY_IDLE=0, PRIORITY_LOWEST, PRIORITY_NORMAL, PRIORITY_CRITICAL };

typedef struct _task_queue_context_t
{
	int32_t ref;
	int running;
	int maxWorker;
	thread_pool_t pool;

	pthread_t thread_scheduler;
	sema_t sema_worker;
	sema_t sema_request;

	locker_t locker;
	struct list_head tasks;
	struct list_head tasks_recycle;
	size_t tasks_count;
	size_t tasks_recycle_count;
} task_queue_context_t;

typedef struct _task_context_t
{
	struct list_head head;

	task_queue_context_t *taskQ;
	int timeout;
	task_proc proc;
	void* param;

	uint64_t stime;
	uint64_t etime;
	tid_t thread;
	int priority;
} task_context_t;

static task_context_t* task_alloc(task_queue_context_t* taskQ)
{
	task_context_t* task;
	if(list_empty(&taskQ->tasks_recycle))
	{
		task = (task_context_t*)malloc(sizeof(task_context_t));
	}
	else
	{
		task = list_entry(taskQ->tasks_recycle.next, task_context_t, head);
		list_remove(taskQ->tasks_recycle.next);
		assert(taskQ->tasks_recycle_count > 0);
		--taskQ->tasks_recycle_count;
	}

	if(task)
	{
		memset(task, 0, sizeof(task_context_t));
		LIST_INIT_HEAD(&task->head);
	}

	return task;
}

static void task_recycle(task_queue_context_t* taskQ, task_context_t* task)
{
	// max recycle count
	if(taskQ->tasks_recycle_count > 500)
	{
		free(task);
		return;
	}

	list_insert_after(&task->head, taskQ->tasks_recycle.prev);
	assert(taskQ->tasks_recycle_count >= 0);
	++taskQ->tasks_recycle_count;
}

static void task_clean(task_queue_context_t* taskQ)
{
	task_context_t *task;
	struct list_head *p, *n;
	list_for_each_safe(p, n, &taskQ->tasks_recycle)
	{
		task = list_entry(p, task_context_t, head);
		free(task);
	}

	list_for_each_safe(p, n, &taskQ->tasks)
	{
		task = list_entry(p, task_context_t, head);
		free(task);
	}
}

static void task_push(task_queue_context_t* taskQ, task_context_t* task)
{
	list_insert_after(&task->head, taskQ->tasks.prev);
	assert(taskQ->tasks_count >= 0);
	++taskQ->tasks_count;
}

static task_context_t* task_pop(task_queue_context_t* taskQ)
{
	task_context_t* task;
	if(list_empty(&taskQ->tasks))
		return NULL;

	assert(taskQ->tasks_count > 0);
	--taskQ->tasks_count;
	task = list_entry(taskQ->tasks.next, task_context_t, head);
	list_remove(taskQ->tasks.next);
	return task;
}

static task_context_t* task_pop_timeout(task_queue_context_t* taskQ)
{
	uint64_t clock;
	task_context_t *task;
	struct list_head *p, *next;

	clock = system_clock();
	list_for_each_safe(p, next, &taskQ->tasks)
	{
		task = list_entry(p, task_context_t, head);
		if(clock - task->stime > (size_t)task->timeout)
		{
			list_remove(p);
			return task;
		}
	}

	return NULL;
}

static void task_queue_relase(task_queue_context_t* taskQ)
{
	if(0 == atomic_decrement32(&taskQ->ref))
	{
		sema_destroy(&taskQ->sema_request);
		sema_destroy(&taskQ->sema_worker);
		locker_destroy(&taskQ->locker);

		task_clean(taskQ);
		free(taskQ);
	}
}

static void task_action(void* param)
{
	task_context_t* task;
	task_queue_context_t* taskQ;
	task = (task_context_t*)param;
	taskQ = task->taskQ;

	if(task->proc)
		task->proc(task->param);

	task->etime = system_clock();
	task->thread = thread_getid(thread_self());
	locker_lock(&taskQ->locker);
	task_recycle(taskQ, task); // recycle task
	locker_unlock(&taskQ->locker);

	sema_post(&taskQ->sema_worker); // add worker
	task_queue_relase(taskQ);
}

static int STDCALL task_queue_scheduler(void* param)
{
	int r;
	task_context_t *task;
	task_queue_context_t *taskQ;

	taskQ = (task_queue_context_t*)param;
	while(taskQ->running)
	{
		r = sema_wait(&taskQ->sema_request);
		r = sema_wait(&taskQ->sema_worker);
		if(0 == r && taskQ->running)
		{
			locker_lock(&taskQ->locker);
			task = task_pop(taskQ);
			locker_unlock(&taskQ->locker);

			assert(task);
			if(task)
			{
				atomic_increment32(&taskQ->ref);
				r = thread_pool_push(taskQ->pool, task_action, task);
				assert(0 == r);
			}
		}
	}

	return 0;
}

int task_queue_post(task_queue_t q, task_proc proc, void* param)
{
	task_context_t* task;
	task_queue_context_t *taskQ;

	taskQ = (task_queue_context_t *)q;
	locker_lock(&taskQ->locker);
	task = task_alloc(taskQ);
	locker_unlock(&taskQ->locker);
	if(!task)
		return -ENOMEM;

	task->taskQ = taskQ;
	task->stime = system_clock();
	task->timeout = 5000;
	task->priority = 0;
	task->proc = proc;
	task->param = param;
	locker_lock(&taskQ->locker);
	task_push(taskQ, task);
	locker_unlock(&taskQ->locker);

	return sema_post(&taskQ->sema_request);
}

task_queue_t task_queue_create(thread_pool_t pool, int maxWorker)
{
	int r;
	task_queue_context_t* taskQ;
	taskQ = (task_queue_context_t*)malloc(sizeof(task_queue_context_t));
	if(taskQ)
	{
		taskQ->ref = 1;
		taskQ->running = 1;
		taskQ->pool = pool;
		taskQ->maxWorker = maxWorker;
		taskQ->tasks_recycle_count = 0;
		LIST_INIT_HEAD(&taskQ->tasks);
		LIST_INIT_HEAD(&taskQ->tasks_recycle);

		r = locker_create(&taskQ->locker);
		assert(0 == r);

		// create worker semaphore
		r = sema_create(&taskQ->sema_worker, NULL, maxWorker);
		assert(0 == r);

		// create request semaphore
		r = sema_create(&taskQ->sema_request, NULL, 0);
		assert(0 == r);

		// create schedule thread
		r = thread_create(&taskQ->thread_scheduler, task_queue_scheduler, taskQ);
		assert(0 == r);
	}
	return taskQ;
}

int task_queue_destroy(task_queue_t q)
{
	task_queue_context_t* taskQ;
	taskQ = (task_queue_context_t*)q;

	// notify exit
	taskQ->running = 0;
	sema_post(&taskQ->sema_request);
	sema_post(&taskQ->sema_worker);
	thread_destroy(taskQ->thread_scheduler);

	task_queue_relase(taskQ);
	return 0;
}
