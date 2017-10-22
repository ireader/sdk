#ifndef _aio_rwutil_
#define _aio_rwutil_

#include "aio-socket.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct aio_socket_rw_t
{
	unsigned char _reserved_[256];
};

int aio_socket_recv_all(struct aio_socket_rw_t* rw, int timeout, aio_socket_t socket, void* buffer, size_t bytes, aio_onrecv proc, void* param);
/// @param[in] vec vec value may be changed, and must be valid until proc callback
int aio_socket_recv_v_all(struct aio_socket_rw_t* rw, int timeout, aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecv proc, void* param);

int aio_socket_send_all(struct aio_socket_rw_t* rw, int timeout, aio_socket_t socket, const void* buffer, size_t bytes, aio_onsend proc, void* param);
/// @param[in] vec vec value may be changed, and must be valid until proc callback
int aio_socket_send_v_all(struct aio_socket_rw_t* rw, int timeout, aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onsend proc, void* param);

#if defined(__cplusplus)
}
#endif
#endif /* !_aio_rwutil_ */
