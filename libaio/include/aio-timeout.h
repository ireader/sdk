#ifndef _aio_timeout_h_
#define _aio_timeout_h_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct aio_timeout_t
{
	uint8_t reserved[64]; // internal use only
};

void aio_timeout_process(void);

/// aio timer start/stop
/// every start MUST call stop once
int aio_timeout_start(struct aio_timeout_t* timeout, int timeoutMS, void(*notify)(void* param), void* param);
/// @return  0-ok, other-timer can't be stop(timer have triggered or will be triggered)
int aio_timeout_stop(struct aio_timeout_t* timeout);

#ifdef __cplusplus
}
#endif
#endif /* !_aio_timeout_h_ */
