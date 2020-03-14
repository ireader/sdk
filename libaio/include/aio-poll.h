#ifndef _aio_poll_h_
#define _aio_poll_h_

#include "sys/sock.h"

#ifdef __cplusplus
extern "C" {
#endif

struct aio_poll_t;

enum
{
	AIO_POLL_IN = 0x01,
	AIO_POLL_OUT = 0x02,
};

struct aio_poll_t* aio_poll_create(void);
int aio_poll_destroy(struct aio_poll_t* poll);

/// Read/Write callback
/// @param[in] code 0-ok, ETIMEOUT-timeout, other-error
/// @param[in] socket same as aio_poll_poll socket
/// @param[in] flags AIO_POLL_IN/AIO_POLL_OUT
/// @param[in] param same as aio_poll_poll param
typedef void (*aio_poll_onpoll)(int code, socket_t socket, int flags, void* param);

/// Async poll
/// @param[in] flags AIO_POLL_IN/AIO_POLL_OUT
/// @param[in] timeout poll timeout, in MS(poll internal precision: 10s)
/// @param[in] callback poll callback
/// @param[in] param user defined callback parameter
/// @return 0-ok, other-error
int aio_poll_poll(struct aio_poll_t* poll, socket_t socket, int flags, int timeout, aio_poll_onpoll callback, void* param);

#ifdef __cplusplus
}
#endif
#endif /* !_aio_poll_h_ */
