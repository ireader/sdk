#ifndef _aio_tcp_transport_h_
#define _aio_tcp_transport_h_

#include "sys/sock.h"

#ifdef __cplusplus
extern "C" {
#endif

struct aio_tcp_transport_handler_t
{
	/// aio transport create
	void (*oncreate)(void* param, void* transport);

	/// aio transport destroy
	void (*ondestroy)(void* param, void* transport);

	/// @param[in] param user-defined pointer return by onconnected
	void (*onrecv)(void* param, void* transport, const void* data, size_t bytes);

	/// @param[in] param user-defined pointer return by onconnected
	void (*onsend)(void* param, void* transport, int code, size_t bytes);
};

void aio_tcp_transport_init(void);
void aio_tcp_transport_clean(void);
void aio_tcp_transport_recycle(void); /// recycle timeout(or error) transport

/// @param[in] timeout recv/send timeout(millisecond), default 4min
void aio_tcp_transport_set_timeout(int timeout);
int aio_tcp_transport_get_timeout(void);

/// create tcp transport
/// @param[in] socket transport socket, hold and close by internal
/// @param[in] handler user-defined callback functions
/// @param[in] param user-defined callback parameter
/// @return 0-ok, other-error(user must close socket)
int aio_tcp_transport_create(socket_t socket, struct aio_tcp_transport_handler_t *handler, void* param);

/// send data to peer
/// @param[in] data use data send to peer, MUST BE VALID until onsend
/// @param[in] bytes data length in byte
/// @return 0-ok, other-error
int aio_tcp_transport_send(void* transport, const void* data, size_t bytes);

/// send data to peer
/// @param[in] vec data vector, MUST BE VALID until onsend
/// @param[in] n vector count
/// @return 0-ok, other-error
int aio_tcp_transport_sendv(void* transport, socket_bufvec_t *vec, int n);

#ifdef __cplusplus
}
#endif
#endif /* !_aio_tcp_transport_h_ */
