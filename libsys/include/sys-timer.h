#ifndef _sys_timer_h_
#define _sys_timer_h_

#include "libsys.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* sys_timer_t;
typedef void (*sys_timer_proc)(sys_timer_t id, void* param);

/// add a timer
/// @param[out] id timer id
/// @param[in] period timer period in ms
/// @param[in] callback timer callback
/// @param[in] cbparam user defined param
/// @return 0-ok, <0-error
LIBSYS_API int sys_timer_oneshot(sys_timer_t *id, unsigned int period, sys_timer_proc callback, void* cbparam);

/// add a timer
/// @param[out] id timer id
/// @param[in] period timer period in ms
/// @param[in] callback timer callback
/// @param[in] cbparam user defined param
/// @return 0-ok, <0-error
LIBSYS_API int sys_timer_start(sys_timer_t *id, unsigned int period, sys_timer_proc callback, void* cbparam);

/// delete a timer
/// @param[in] id timer id
/// @return 0-ok, <0-error
LIBSYS_API int sys_timer_stop(sys_timer_t id);

#ifdef __cplusplus
}
#endif

#endif /* !_sys_timer_h_ */
