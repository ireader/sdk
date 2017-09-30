#include "aio-timeout.h"
#include "sys/locker.h"
#include "sys/atomic.h"
#include "sys/system.h"
#include "sys/onetime.h"
#include "timer.h"
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

	timer_process(s_timer, system_clock());
}

int aio_timeout_start(struct aio_timeout_t* timeout, int timeoutMS, void (*notify)(void* param), void* param)
{
	onetime_exec(&s_init, aio_timeout_init);

	timeout->timeout.param = param;
	timeout->timeout.ontimeout = notify;
	timeout->timeout.expire = system_clock() + timeoutMS;
	return timer_start(s_timer, &timeout->timeout, timeout->timeout.expire - timeoutMS);
}

int aio_timeout_stop(struct aio_timeout_t* timeout)
{
	return timer_stop(s_timer, &timeout->timeout);
}
