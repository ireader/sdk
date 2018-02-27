#ifndef _twtimer_h_
#define _twtimer_h_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct twtimer_t
{
	uint64_t expire; // expire clock time

	struct twtimer_t* next;
	struct twtimer_t** pprev;

	void (*ontimeout)(void* param);
	void* param;
};

typedef struct time_wheel_t time_wheel_t;
time_wheel_t* time_wheel_create(uint64_t clock);
int time_wheel_destroy(time_wheel_t* tm);

/// @return sleep time(ms)
int twtimer_process(time_wheel_t* tm, uint64_t clock);

/// one-shoot timeout timer
/// @return 0-ok, other-error
int twtimer_start(time_wheel_t* tm, struct twtimer_t* timer);
/// @return  0-ok, other-timer can't be stop(timer have triggered or will be triggered)
int twtimer_stop(time_wheel_t* tm, struct twtimer_t* timer);

#if defined(__cplusplus)
}
#endif
#endif /* !_twtimer_h_ */
