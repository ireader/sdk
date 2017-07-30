#include "aio-timeout.h"
#include "sys/locker.h"
#include "sys/atomic.h"
#include "sys/system.h"
#include "sys/onetime.h"
#include <assert.h>

static struct
{
	locker_t locker;
	struct aio_timeout_t root;
} s_list;
static onetime_t s_init = ONETIME_INIT;

static void aio_timeout_release(struct aio_timeout_t* timeout)
{
	if (0 == atomic_decrement32(&timeout->ref))
	{
		if (timeout->cancel)
			timeout->cancel(timeout->param2);
	}
}

static void aio_timeout_init(void)
{
	assert(NULL == s_list.root.next);
	s_list.root.next = s_list.root.prev = &s_list.root;
	locker_create(&s_list.locker);
}

//static void aio_timeout_clean(void)
//{
//	locker_destroy(&s_list.locker);
//}

void aio_timeout_process(void)
{
	uint64_t clock;
	struct aio_timeout_t *p;
	struct aio_timeout_t *prev, *next;

	onetime_exec(&s_init, aio_timeout_init);
	clock = system_clock();

	do
	{
		locker_lock(&s_list.locker);
		for (p = s_list.root.next; p != &s_list.root; p = p->next)
		{
			if (-1 == p->clock/*cancel*/ || (p->enable && clock > p->clock + p->timeout))
			{
				next = p->next;
				prev = p->prev;
				prev->next = next;
				next->prev = prev;
				p->next = NULL;
				p->prev = NULL;
				break; // find timeout item
			}
		}
		locker_unlock(&s_list.locker);

		if (p != &s_list.root)
		{
			p->notify(p->param);
			aio_timeout_release(p);
		}
	} while (p != &s_list.root);
}

void aio_timeout_add(struct aio_timeout_t* timeout, int timeoutMS, void (*notify)(void* param), void* param)
{
	struct aio_timeout_t* prev, *next;

	onetime_exec(&s_init, aio_timeout_init);

	timeout->ref = 2;
	timeout->clock = 0;
	timeout->enable = 0;
	timeout->timeout = timeoutMS;
	timeout->notify = notify;
	timeout->param = param;
	assert(timeout->notify);

	locker_lock(&s_list.locker);
	next = &s_list.root;
	prev = s_list.root.prev;
	timeout->next = next;
	timeout->prev = prev;
	prev->next = timeout;
	next->prev = timeout;
	locker_unlock(&s_list.locker);
}

void aio_timeout_delete(struct aio_timeout_t* timeout, void(*cancel)(void* param), void* param)
{
	struct aio_timeout_t *prev, *next;

	locker_lock(&s_list.locker);
	next = timeout->next;
	prev = timeout->prev;
	if (next && prev)
	{
		prev->next = next;
		next->prev = prev;
		timeout->next = NULL;
		timeout->prev = NULL;
	}
	locker_unlock(&s_list.locker);

	if (next && prev)
	{
		//timeout->notify(timeout->param, 1);
		assert(2 == timeout->ref);
		aio_timeout_release(timeout);
	}

	timeout->cancel = cancel;
	timeout->param2 = param;
	aio_timeout_release(timeout);
}

void aio_timeout_settimeout(struct aio_timeout_t* timeout, int ms)
{
	timeout->timeout = ms;
}

void aio_timeout_start(struct aio_timeout_t* timeout)
{
	timeout->clock = system_clock();
	timeout->enable = 1;
}

void aio_timeout_stop(struct aio_timeout_t* timeout)
{
	timeout->enable = 0;
}
