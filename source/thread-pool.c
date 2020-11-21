#include "thread-pool.h"
#include "sys/locker.h"
#include "sys/system.h"
#include "sys/thread.h"
#include "sys/event.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct _thread_pool_context_t;
typedef struct _thread_list_t
{
	struct _thread_list_t *next;
	struct _thread_pool_context_t *pool;
	pthread_t thread;
} thread_list_t;

typedef struct _thread_task_list_t
{
	struct _thread_task_list_t *next;
	thread_pool_proc proc;
	void *param;
} thread_task_list_t;

typedef struct _thread_pool_context_t
{
	int run;
	int idle_max;
	int threshold;

	int thread_count;
	int thread_count_min;
	int thread_count_max;
	int thread_count_idle;

	int task_count;
	thread_task_list_t *tasks;
	thread_task_list_t *recycle_tasks;

	thread_list_t *task_threads;

	locker_t locker;
	event_t event;

} thread_pool_context_t;

static void thread_pool_destroy_thread(thread_pool_context_t *context);

static int STDCALL thread_pool_worker(void *param)
{
	thread_list_t* threads;
	thread_task_list_t *task;
	thread_pool_context_t *context;

	threads = (thread_list_t*)param;
	context = threads->pool;

	locker_lock(&context->locker);
	while(context->run)
	{
		// pop task
		task = context->tasks;
		while(task && context->run)
		{
			// remove task from task list
			context->tasks = task->next;
			--context->task_count;

			// do task procedure
			--context->thread_count_idle;
			locker_unlock(&context->locker);
			task->proc(task->param);
			locker_lock(&context->locker);
			++context->thread_count_idle;

			// recycle task: push task to recycle list
			task->next = context->recycle_tasks;
			context->recycle_tasks = task;

			// do next
			task = context->tasks;
		}

		// delete idle thread
		if(context->thread_count_idle > context->idle_max
			|| !context->run)
			break;

		// wait for task
		locker_unlock(&context->locker);
		event_timewait(&context->event, 60*1000);
		locker_lock(&context->locker);
	}

	--context->thread_count;
	--context->thread_count_idle;
	thread_pool_destroy_thread(context);
	locker_unlock(&context->locker);

	return 0;
}

static thread_list_t* thread_pool_create_thread(thread_pool_context_t *context)
{
	thread_list_t* threads;
	
	threads = (thread_list_t*)malloc(sizeof(thread_list_t));
	if(!threads)
		return NULL;

	memset(threads, 0, sizeof(thread_list_t));
	threads->pool = context;

	if(0 != thread_create(&threads->thread, thread_pool_worker, threads))
	{
		free(threads);
		return NULL;
	}

	return threads;
}

static void thread_pool_destroy_thread(thread_pool_context_t *context)
{
	thread_list_t **head;
	thread_list_t *next;

	head = &context->task_threads;
	while(*head)
	{
		if(thread_isself((*head)->thread))
		{
			next = *head;
			*head = (*head)->next;
			free(next);
			break;
		}
		head = &(*head)->next;
	}
}

static void thread_pool_create_threads(thread_pool_context_t *context, 
									   int num)
{
	int i;
	thread_list_t *threads;

	for(i=0; i<num; i++)
	{
		threads = thread_pool_create_thread(context);
		if(!threads)
			break;

		// add to list head
		threads->next = context->task_threads;
		context->task_threads = threads;
	}

	context->thread_count += i;
	context->thread_count_idle += i;
}

static void thread_pool_destroy_threads(thread_list_t *threads)
{
	thread_list_t *next;

	while(threads)
	{
		next = threads->next;
		thread_destroy(threads->thread);
		free(threads);
		threads = next;
	}
}

static thread_task_list_t* thread_pool_create_task(thread_pool_context_t *context,
												   thread_pool_proc proc,
												   void* param)
{
	thread_task_list_t *task;

	if(context->recycle_tasks)
	{
		task = context->recycle_tasks;
		context->recycle_tasks = context->recycle_tasks->next;
	}
	else
	{
		task = (thread_task_list_t*)malloc(sizeof(thread_task_list_t));
	}
	if(!task)
		return NULL;
	
	memset(task, 0, sizeof(thread_task_list_t));
	task->param = param;
	task->proc = proc;
	return task;
}

static void thread_pool_destroy_tasks(thread_task_list_t *tasks)
{
	thread_task_list_t *next;

	while(tasks)
	{
		next = tasks->next;
		free(tasks);
		tasks = next;
	}
}

thread_pool_t thread_pool_create(int num, int min, int max)
{
	thread_pool_context_t *ctx;

	ctx = (thread_pool_context_t*)malloc(sizeof(thread_pool_context_t));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(thread_pool_context_t));
	ctx->thread_count_min = min;
	ctx->thread_count_max = max;
	ctx->threshold = num / 2;
	ctx->idle_max = num;
	ctx->run = 1;

	if(0 != locker_create(&ctx->locker))
	{
		free(ctx);
		return NULL;
	}

	if(0 != event_create(&ctx->event))
	{
		locker_destroy(&ctx->locker);
		free(ctx);
		return NULL;
	}

	thread_pool_create_threads(ctx, num);

	return ctx;
}

void thread_pool_destroy(thread_pool_t pool)
{
	thread_pool_context_t *ctx;

	ctx = (thread_pool_context_t*)pool;
	ctx->run = 0;

	locker_lock(&ctx->locker);
	while(ctx->thread_count)
	{
		event_signal(&ctx->event);
		locker_unlock(&ctx->locker);
		system_sleep(100);
		locker_lock(&ctx->locker);
	}
	locker_unlock(&ctx->locker);
	
	//thread_pool_destroy_threads(ctx->task_threads);
	thread_pool_destroy_tasks(ctx->recycle_tasks);
	thread_pool_destroy_tasks(ctx->tasks);

	event_destroy(&ctx->event);
	locker_destroy(&ctx->locker);
	free(ctx);
}

int thread_pool_threads_count(thread_pool_t pool)
{
	thread_pool_context_t *ctx;

	ctx = (thread_pool_context_t*)pool;
	return ctx->thread_count;
}

int thread_pool_push(thread_pool_t pool, thread_pool_proc proc, void *param)
{
	thread_task_list_t *task;
	thread_pool_context_t *context;

	context = (thread_pool_context_t*)pool;

	locker_lock(&context->locker);
	task = thread_pool_create_task(context, proc, param);
	if(!task)
	{
		locker_unlock(&context->locker);
		return -1;
	}

	// add to task list
	task->next = context->tasks;
	context->tasks = task;
	++context->task_count;

	// add new thread to do task
	if(context->thread_count_idle<1 
		&& context->thread_count<context->thread_count_max)
		//&& context->task_count>context->threshold)
		thread_pool_create_threads(context, 1);

	event_signal(&context->event);
	locker_unlock(&context->locker);
	return 0;
}
