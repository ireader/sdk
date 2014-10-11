#ifndef _aio_udp_transport_h_
#define _aio_udp_transport_h_

#include "sys/sock.h"

struct aio_udp_transport_handler_t
{
	/// param[in] ptr user-defined pointer input from aio_udp_transport_create ptr parameter 
	/// param[in] session transport session parameter. use with send/sendv/addref/release
	/// param[out] user user-defined pointer, use for onsend
	void (*onrecv)(void* ptr, void* session, const void* msg, size_t bytes, const char* ip, int port, void** user);

	/// param[in] user user-defined pointer(onrecv parameter)
	void (*onsend)(void* ptr, void* session, void* user, int code, size_t bytes);
};

void* aio_udp_transport_create(socket_t socket, const struct aio_udp_transport_handler_t *handler, void* ptr);
int aio_udp_transport_destroy(void* transport);

int aio_udp_transport_send(void* session, const void* msg, size_t bytes);
int aio_udp_transport_sendv(void* session, socket_bufvec_t *vec, int n);

int aio_udp_transport_addref(void* session);
int aio_udp_transport_release(void* session);

#endif /* !_aio_udp_transport_h_ */
