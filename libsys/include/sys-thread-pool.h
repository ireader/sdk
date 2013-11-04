#ifndef _sys_thread_pool_h_
#define _sys_thread_pool_h_

#include "libsys.h"

#ifdef __cplusplus
extern "C" {
#endif

///thread pool procedure
///@param[in] param user parameter
typedef void (*sys_thread_pool_proc)(void *param);

///push a task to thread pool
///@param[in] proc task procedure
///@param[in] param user parameter
///@return =0-ok, <0-error code
LIBSYS_API int sys_thread_pool_push(sys_thread_pool_proc proc, void *param);

#ifdef __cplusplus
}
#endif

#endif /* !_sys_thread_pool_h_ */
