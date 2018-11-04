#include "work-queue.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "sys/onetime.h"
#include "sys/sema.h"
#include "rarray.h"
#include "list.h"

enum job_status_t
{
    JOB_STATUS_IDLE,
    JOB_STATUS_WORKING,
};

struct work_queue_t
{
    struct list_head link;
    work_queue_worker worker;
    void* param;

    int32_t working; // JOB_STATUS_xxx
    locker_t locker;
    struct rarray_t jobs;
};

struct job_dispatch_t
{
    struct list_head root;
    locker_t locker;
    sema_t sema;
};

static onetime_t s_once = ONETIME_INIT;
static struct job_dispatch_t s_dispatcher;

static void queue_dispatch_create(void)
{
    struct job_dispatch_t* dispatcher = &s_dispatcher;
    LIST_INIT_HEAD(&dispatcher->root);
    locker_create(&dispatcher->locker);
    sema_create(&dispatcher->sema, NULL, 0);
}

static void queue_dispatch_destroy(struct job_dispatch_t* dispatcher)
{
    assert(list_empty(&dispatcher->root));
    locker_destroy(&dispatcher->locker);
    sema_destroy(&dispatcher->sema);
}

static void queue_dispatch_push(struct list_head* node)
{
    struct job_dispatch_t* dispatcher = &s_dispatcher;
    locker_lock(&dispatcher->locker);
    list_insert_after(node, dispatcher->root.prev);
    locker_unlock(&dispatcher->locker);
    sema_post(&dispatcher->sema);
}

struct work_queue_t* work_queue_dispatch(int timeoutMS)
{
    struct work_queue_t* node;
    struct job_dispatch_t* dispatcher = &s_dispatcher;
    if (0 != sema_timewait(&dispatcher->sema, timeoutMS))
        return NULL;

    locker_lock(&dispatcher->locker);
    assert(!list_empty(&dispatcher->root));
    node = list_entry(dispatcher->root.next, struct work_queue_t, link);
    list_remove(&node->link);
    locker_unlock(&dispatcher->locker);
    return node;
}

void work_queue_process(struct work_queue_t* q)
{
    q->worker(q->param, q);
}

struct work_queue_t* work_queue_create(int size, int capacity, work_queue_worker worker, void* param)
{
    struct work_queue_t* q;
    q = (struct work_queue_t*)calloc(1, sizeof(*q));
    if (!q) return NULL;

    locker_create(&q->locker);
    q->working = JOB_STATUS_IDLE;
    q->worker = worker;
    q->param = param;

    if (0 != rarray_init(&q->jobs, size, capacity))
    {
        work_queue_destroy(q);
        return NULL;
    }

    onetime_exec(&s_once, queue_dispatch_create);
    return q;
}

void work_queue_destroy(struct work_queue_t* q)
{
    if (q)
    {
        locker_destroy(&q->locker);
        rarray_free(&q->jobs);
        free(q);
    }
}

void work_queue_clear(struct work_queue_t* q)
{
    locker_lock(&q->locker);
    rarray_clear(&q->jobs);
    locker_unlock(&q->locker);
}

int work_queue_count(struct work_queue_t* q)
{
    int r;
    locker_lock(&q->locker);
    r = rarray_count(&q->jobs);
    locker_unlock(&q->locker);
    return r;
}

int work_queue_push(struct work_queue_t* q, const void* job)
{
    int r;
    locker_lock(&q->locker);
    r = rarray_push_back(&q->jobs, job);
    if (0 == r)
    {
        if (JOB_STATUS_IDLE == q->working)
        {
            q->working = JOB_STATUS_WORKING;
            queue_dispatch_push(&q->link);
        }
    }
    locker_unlock(&q->locker);
    return r;
}

const void* work_queue_pop(struct work_queue_t* q)
{
    const void* job;
    locker_lock(&q->locker);
    assert(JOB_STATUS_WORKING == q->working);
    job = rarray_front(&q->jobs);
    if (job)
    {
        rarray_pop_front(&q->jobs);
    }
    else
    {
        q->working = JOB_STATUS_IDLE;
    }
    locker_unlock(&q->locker);
    return job;
}
