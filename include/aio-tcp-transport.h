#ifndef _aio_tcp_transport_h_
#define _aio_tcp_transport_h_

#include "sys/sock.h"

struct aio_tcp_transport_handler_t
{
	/// param[in] ptr user-defined pointer input from aio_tcp_transport_create ptr parameter 
	/// param[in] session transport session parameter. use with send/sendv/addref/release
	/// @return user-defined pointer
	void* (*onconnected)(void* ptr, void* session, const struct sockaddr* sa, socklen_t salen);

	/// param[in] data user-defined pointer return by onconnected
	/// @return 1-receive more data, 0-don't receive
	int (*onrecv)(void* user, const void* msg, size_t bytes);

	/// param[in] data user-defined pointer return by onconnected
	/// @return 1-receive more data, 0-don't receive
	int (*onsend)(void* user, int code, size_t bytes);

	/// param[in] data user-defined pointer return by onconnected
	void (*ondisconnected)(void* user);
};

void* aio_tcp_transport_create(socket_t socket, const struct aio_tcp_transport_handler_t *handler, void* ptr);
int aio_tcp_transport_destroy(void* transport);

int aio_tcp_transport_send(void* session, const void* msg, size_t bytes);
int aio_tcp_transport_sendv(void* session, socket_bufvec_t *vec, int n);

int aio_tcp_transport_disconnect(void* session);

// recycle idle session
int aio_tcp_transport_recycle(void* transport);

#endif /* !_aio_tcp_transport_h_ */
