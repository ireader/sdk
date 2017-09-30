#include "aio-send.h"

static void aio_send_timeout(void* param)
{
	struct aio_send_t* send;
	send = (struct aio_send_t*)param;

	if (send->onsend)
		send->onsend(send->param, ETIMEDOUT, 0);
}

static void aio_send_handler(void* param, int code, size_t bytes)
{
	struct aio_send_t* send;
	send = (struct aio_send_t*)param;
	if (0 != aio_timeout_stop(&send->timeout))
		return;

	if (send->onsend)
		send->onsend(send->param, code, bytes);
}

int aio_send(struct aio_send_t* send, int timeout, aio_socket_t aio, const void* buffer, size_t bytes, aio_onsend onsend, void* param)
{
	send->param = param;
	send->onsend = onsend;
	aio_timeout_start(&send->timeout, timeout, aio_send_timeout, send);
	return aio_socket_send(aio, buffer, bytes, aio_send_handler, send);
}

int aio_send_v(struct aio_send_t* send, int timeout, aio_socket_t aio, socket_bufvec_t* vec, int n, aio_onsend onsend, void* param)
{
	send->param = param;
	send->onsend = onsend;
	aio_timeout_start(&send->timeout, timeout, aio_send_timeout, send);
	return aio_socket_send_v(aio, vec, n, aio_send_handler, send);
}

int aio_sendto(struct aio_send_t* send, int timeout, aio_socket_t aio, const struct sockaddr *addr, socklen_t addrlen, void* buffer, size_t bytes, aio_onsend onsend, void* param)
{
	send->param = param;
	send->onsend = onsend;
	aio_timeout_start(&send->timeout, timeout, aio_send_timeout, send);
	return aio_socket_sendto(aio, addr, addrlen, buffer, bytes, aio_send_handler, send);
}

int aio_sendto_v(struct aio_send_t* send, int timeout, aio_socket_t aio, const struct sockaddr *addr, socklen_t addrlen, socket_bufvec_t* vec, int n, aio_onsend onsend, void* param)
{
	send->param = param;
	send->onsend = onsend;
	aio_timeout_start(&send->timeout, timeout, aio_send_timeout, send);
	return aio_socket_sendto_v(aio, addr, addrlen, vec, n, aio_send_handler, send);
}
