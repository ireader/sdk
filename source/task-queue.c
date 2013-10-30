#include "task-queue.h"
#include "cstringext.h"
#include "sys/process.h"
#include "sys/sync.h"
#include "time64.h"
#include "thread-pool.h"
#include "list.h"
#include <errno.h>

enum { PRIORITY_IDLE=0, PRIORITY_LOWEST, PRIORITY_NORMAL, PRIORITY_CRITICAL };

typedef struct
{
	struct list_head head;

	task_t id;
	int timeout;
	task_proc proc;
	void* param;
	
	time64_t stime;
	time64_t etime;
	int thread;
	int priority;
} task_context_t;

typedef struct
{
	int running;
	int maxWorker;
	thread_pool_t pool;

	thread_t thread_scheduler;
	semaphore_t sema_worker;
	semaphore_t sema_request;

	locker_t locker;
	struct list_head tasks;
	struct list_head tasks_recycle;
	size_t tasks_count;
	size_t tasks_recycle_count;
} task_queue_context_t;

static task_queue_context_t g_ctx;

static task_context_t* task_alloc()
{
	task_context_t* task;
	if(list_empty(&g_ctx.tasks_recycle))
	{
		task = (task_context_t*)malloc(sizeof(task_context_t));
	}
	else
	{
		task = list_entry(g_ctx.tasks_recycle.next, task_context_t, head);
		list_remove(g_ctx.tasks_recycle.next);
		assert(g_ctx.tasks_recycle_count > 0);
		--g_ctx.tasks_recycle_count;
	}

	if(task)
	{
		memset(task, 0, sizeof(task_context_t));
		LIST_INIT_HEAD(&task->head);
	}

	return task;
}

static void task_recycle(task_context_t* task)
{
	// max recycle count
	if(g_ctx.tasks_recycle_count > 500)
	{
		free(task);
		return;
	}

	list_insert_after(&task->head, g_ctx.tasks_recycle.prev);
	assert(g_ctx.tasks_recycle_count >= 0);
	++g_ctx.tasks_recycle_count;
}

static void task_clean()
{
	task_context_t *task;
	struct list_head *p, *n;
	list_for_each_safe(p, n, &g_ctx.tasks_recycle)
	{
		task = list_entry(p, task_context_t, head);
		free(task);
	}
}

static void task_push(task_context_t* task)
{
	list_insert_after(&task->head, g_ctx.tasks.prev);
	assert(g_ctx.tasks_count >= 0);
	++g_ctx.tasks_count;
}

static task_context_t* task_pop()
{
	task_context_t* task;
	if(list_empty(&g_ctx.tasks))
		return NULL;

	assert(g_ctx.tasks_count > 0);
	--g_ctx.tasks_count;
	task = list_entry(g_ctx.tasks.next, task_context_t, head);
	list_remove(g_ctx.tasks.next);
	return task;
}

static void task_action(void* param)
{
	task_context_t* task;
	task = (task_context_t*)param;

	if(task->proc)
		task->proc(task->id, task->param);

	task->etime = time64_now();
	task->thread = thread_self();
	locker_lock(&g_ctx.locker);
	task_recycle(task); // recycle task
	locker_unlock(&g_ctx.locker);

	semaphore_post(&g_ctx.sema_worker); // add worker
}

static int task_queue_scheduler(void* param)
{
	int r;
	time64_t tnow;
	task_context_t *task;
	struct list_head *p, *next;

	while(g_ctx.running)
	{
		r = semaphore_wait(&g_ctx.sema_request);
		r = semaphore_timewait(&g_ctx.sema_worker, 1000);
		if(0 == r)
		{
			locker_lock(&g_ctx.locker);
			task = task_pop();
			locker_unlock(&g_ctx.locker);

			if(task)
			{
				r = thread_pool_push(g_ctx.pool, task_action, task);
				assert(0 == r);
				if(0 == r) continue;
			}

			semaphore_post(&g_ctx.sema_worker);
		}
		else
		{
			// timeout
			tnow = time64_now();

			locker_lock(&g_ctx.locker);
			list_for_each_safe(p, next, &g_ctx.tasks)
			{
				task = list_entry(p, task_context_t, head);
				if(task->stime + task->timeout > tnow)
				{
					if(task->proc)
						task->proc(task->id, task->param);
					list_remove(p);
				}
			}
			locker_unlock(&g_ctx.locker);
		}
	}

	return 0;
}

int task_queue_post(task_t id, task_proc proc, void* param)
{
	task_context_t* task;
	locker_lock(&g_ctx.locker);
	task = task_alloc();
	locker_unlock(&g_ctx.locker);
	if(!task)
		return -ENOMEM;

	task->id = id;
	task->stime = time64_now();
	task->timeout = 5000;
	task->priority = 0;
	task->proc = proc;
	task->param = param;
	locker_lock(&g_ctx.locker);
	task_push(task);
	locker_unlock(&g_ctx.locker);

	return semaphore_post(&g_ctx.sema_request);
}

int task_queue_create(thread_pool_t pool, int maxWorker)
{
	int r;

	g_ctx.running = 1;
	g_ctx.pool = pool;
	g_ctx.maxWorker = maxWorker;
	g_ctx.tasks_recycle_count = 0;
	LIST_INIT_HEAD(&g_ctx.tasks);
	LIST_INIT_HEAD(&g_ctx.tasks_recycle);

	r = locker_create(&g_ctx.locker);
	assert(0 == r);

	// create worker semaphore
	r = semaphore_create(&g_ctx.sema_worker, NULL, maxWorker);
	assert(0 == r);

	// create request semaphore
	r = semaphore_create(&g_ctx.sema_request, NULL, 0);
	assert(0 == r);

	// create schedule thread
	return thread_create(&g_ctx.thread_scheduler, task_queue_scheduler, NULL);
}

int task_queue_destroy()
{
	// notify exit
	g_ctx.running = 0;
	semaphore_post(&g_ctx.sema_request);
	semaphore_post(&g_ctx.sema_worker);

	thread_destroy(g_ctx.thread_scheduler);
	semaphore_destroy(&g_ctx.sema_request);
	semaphore_destroy(&g_ctx.sema_worker);
	locker_destroy(&g_ctx.locker);

	task_clean();
	return 0;
}
