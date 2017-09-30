#ifndef _timer_h_
#define _timer_h_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct timer_t
{
	uint64_t expire; // expire clock time

	struct timer_t* next;
	struct timer_t** pprev;

	void (*ontimeout)(void* param);
	void* param;
};

typedef struct time_wheel_t time_wheel_t;
time_wheel_t* time_wheel_create(uint64_t clock);
int time_wheel_destroy(time_wheel_t* tm);

/// @return sleep time(ms)
int timer_process(time_wheel_t* tm, uint64_t clock);

/// one-shoot timeout timer
/// @return 0-ok, other-error
int timer_start(time_wheel_t* tm, struct timer_t* timer, uint64_t clock);
/// @return  0-ok, other-timer can't be stop(timer have triggered or will be triggered)
int timer_stop(time_wheel_t* tm, struct timer_t* timer);

#if defined(__cplusplus)
}
#endif
#endif /* !_timer_h_ */
