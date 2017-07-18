#ifndef _aio_tcp_transport_h_
#define _aio_tcp_transport_h_

#include "sys/sock.h"

#ifdef __cplusplus
extern "C" {
#endif

struct aio_tcp_transport_handler_t
{
	/// aio transport destroy
	void (*ondestroy)(void* param);

	/// @param[in] param user-defined pointer return by onconnected
	void (*onrecv)(void* param, const void* data, size_t bytes);

	/// @param[in] param user-defined pointer return by onconnected
	void (*onsend)(void* param, int code, size_t bytes);
};

void aio_tcp_transport_init(void);
void aio_tcp_transport_clean(void);
void aio_tcp_transport_recycle(void); /// recycle timeout transport

/// Create tcp transport
/// @param[in] socket transport socket, hold and close by internal
/// @param[in] handler user-defined callback functions
/// @param[in] param user-defined callback parameter
/// @return NULL-error(user must close socket), other-ok
void* aio_tcp_transport_create(socket_t socket, struct aio_tcp_transport_handler_t *handler, void* param);

/// Destroy tcp transport [OPTIONAL]
/// aio_tcp_transport_destroy take action as aio_tcp_transport_recycle
int aio_tcp_transport_destroy(void* transport);

/// Send data to peer
/// @param[in] data use data send to peer, MUST BE VALID until onsend
/// @param[in] bytes data length in byte
/// @return 0-ok, other-error
int aio_tcp_transport_send(void* transport, const void* data, size_t bytes);

/// Send data to peer
/// @param[in] vec data vector, MUST BE VALID until onsend
/// @param[in] n vector count
/// @return 0-ok, other-error
int aio_tcp_transport_sendv(void* transport, socket_bufvec_t *vec, int n);

/// @param[in] timeout recv/send timeout(millisecond), default 4min
void aio_tcp_transport_set_timeout(void* transport, int timeout);
int aio_tcp_transport_get_timeout(void* transport);

#ifdef __cplusplus
}
#endif
#endif /* !_aio_tcp_transport_h_ */
