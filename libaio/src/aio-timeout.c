#include "aio-timeout.h"
#include "sys/locker.h"
#include "sys/atomic.h"
#include "sys/system.h"
#include "sys/onetime.h"
#include "twtimer.h"
#include <assert.h>

static time_wheel_t* s_timer;
static onetime_t s_init = ONETIME_INIT;

static void aio_timeout_init(void)
{
	s_timer = time_wheel_create(system_clock());
}

static void aio_timeout_clean(void)
{
	time_wheel_destroy(s_timer);
}

void aio_timeout_process(void)
{
	onetime_exec(&s_init, aio_timeout_init);

	twtimer_process(s_timer, system_clock());
}

int aio_timeout_start(struct aio_timeout_t* timeout, int timeoutMS, void (*notify)(void* param), void* param)
{
	struct twtimer_t* timer;
	timer = (struct twtimer_t*)timeout->reserved;
	assert(sizeof(struct twtimer_t) <= sizeof(timeout->reserved));

	onetime_exec(&s_init, aio_timeout_init);

	timer->param = param;
	timer->ontimeout = notify;
	timer->expire = system_clock() + timeoutMS;
	return twtimer_start(s_timer, timer);
}

int aio_timeout_stop(struct aio_timeout_t* timeout)
{
	struct twtimer_t* timer;
	timer = (struct twtimer_t*)timeout->reserved;
	assert(sizeof(struct twtimer_t) <= sizeof(timeout->reserved));
	return twtimer_stop(s_timer, timer);
}
