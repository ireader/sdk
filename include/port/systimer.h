#ifndef _platform_timer_h_
#define _platform_timer_h_

#include "thread-pool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* systimer_t;
typedef void (*systimer_proc)(systimer_t id, void* param);

/// timer initialize
/// @return 0-ok, <0-error
int systimer_init(thread_pool_t pool);

/// timer finalize
/// @return 0-ok, <0-error
int systimer_clean(void);

/// add a timer
/// @param[out] id timer id
/// @param[in] period timer period in ms
/// @param[in] callback timer callback
/// @param[in] cbparam user defined param
/// @return 0-ok, <0-error
int systimer_oneshot(systimer_t *id, unsigned int period, systimer_proc callback, void* cbparam);

/// add a timer
/// @param[out] id timer id
/// @param[in] period timer period in ms
/// @param[in] callback timer callback
/// @param[in] cbparam user defined param
/// @return 0-ok, <0-error
int systimer_start(systimer_t *id, unsigned int period, systimer_proc callback, void* cbparam);

/// delete a timer
/// @param[in] id timer id
/// @return 0-ok, <0-error
int systimer_stop(systimer_t id);

#ifdef __cplusplus
}
#endif

#endif /* !_platform_timer_h_ */
