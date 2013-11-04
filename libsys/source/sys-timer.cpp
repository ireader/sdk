#include "sys-timer.h"
#include "systimer.h"

int sys_timer_oneshot(sys_timer_t *id, unsigned int period, sys_timer_proc callback, void* cbparam)
{
	return systimer_oneshot(id, period, callback, cbparam);
}

int sys_timer_start(sys_timer_t *id, unsigned int period, sys_timer_proc callback, void* cbparam)
{
	return systimer_start(id, period, callback, cbparam);
}

int sys_timer_stop(sys_timer_t id)
{
	return systimer_stop(id);
}
