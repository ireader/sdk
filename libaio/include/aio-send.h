#ifndef _aio_send_h_
#define _aio_send_h_

#include "aio-socket.h"
#include "aio-timeout.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct aio_send_t
{
	struct aio_timeout_t timeout;
	aio_onsend onsend;
	void* param;
};

int aio_send(struct aio_send_t* recv, int timeout, aio_socket_t aio, const void* buffer, size_t bytes, aio_onsend onsend, void* param);
int aio_send_v(struct aio_send_t* recv, int timeout, aio_socket_t aio, socket_bufvec_t* vec, int n, aio_onsend onsend, void* param);
int aio_sendto(struct aio_send_t* recv, int timeout, aio_socket_t aio, const struct sockaddr *addr, socklen_t addrlen, const void* buffer, size_t bytes, aio_onsend onsend, void* param);
int aio_sendto_v(struct aio_send_t* recv, int timeout, aio_socket_t aio, const struct sockaddr *addr, socklen_t addrlen, socket_bufvec_t* vec, int n, aio_onsend onsend, void* param);

#if defined(__cplusplus)
}
#endif
#endif /* !_aio_send_h_ */
