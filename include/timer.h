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

int timer_init(uint64_t clock);
int timer_clean(void);

/// @return 0-ok, other-error
int timer_start(struct timer_t* timer, uint64_t clock);
void timer_stop(struct timer_t* timer);

/// @return sleep time(ms)
int timer_process(uint64_t clock);

#if defined(__cplusplus)
}
#endif
#endif /* !_timer_h_ */
