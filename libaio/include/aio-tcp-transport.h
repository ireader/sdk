#ifndef _aio_tcp_transport_h_
#define _aio_tcp_transport_h_

#include "aio-socket.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct aio_tcp_transport_t aio_tcp_transport_t;

struct aio_tcp_transport_handler_t
{
	/// aio transport destroy
	void (*ondestroy)(void* param);

	/// @param[in] param user-defined pointer
	void (*onrecv)(void* param, int code, size_t bytes);

	/// @param[in] param user-defined pointer
	void (*onsend)(void* param, int code, size_t bytes);
};

/// Create tcp transport
/// @param[in] socket transport socket, hold and close by internal
/// @param[in] handler user-defined callback functions
/// @param[in] param user-defined callback parameter
/// @return NULL-error(user must close socket), other-ok
aio_tcp_transport_t* aio_tcp_transport_create(socket_t socket, struct aio_tcp_transport_handler_t *handler, void* param);
aio_tcp_transport_t* aio_tcp_transport_create2(aio_socket_t aio, struct aio_tcp_transport_handler_t *handler, void* param);

/// cancel tcp transport recv/send
int aio_tcp_transport_destroy(aio_tcp_transport_t* transport);

/// recv data
int aio_tcp_transport_recv(aio_tcp_transport_t* transport, void* data, size_t bytes);
int aio_tcp_transport_recv_v(aio_tcp_transport_t* transport, socket_bufvec_t *vec, int n);

/// Send data to peer
/// @param[in] data use data send to peer, MUST BE VALID until onsend
/// @param[in] bytes data length in byte
/// @return 0-ok, other-error
int aio_tcp_transport_send(aio_tcp_transport_t* transport, const void* data, size_t bytes);

/// Send data to peer
/// @param[in] vec data vector, MUST BE VALID until onsend
/// @param[in] n vector count
/// @return 0-ok, other-error
int aio_tcp_transport_send_v(aio_tcp_transport_t* transport, socket_bufvec_t *vec, int n);

/// @param[in] recvMS recv/send timeout(millisecond), default 4min, 0-infinite
void aio_tcp_transport_set_timeout(aio_tcp_transport_t* transport, int recvMS, int sendMS);
void aio_tcp_transport_get_timeout(aio_tcp_transport_t* transport, int *recvMS, int* sendMS);

#ifdef __cplusplus
}
#endif
#endif /* !_aio_tcp_transport_h_ */
