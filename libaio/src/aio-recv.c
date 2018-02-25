#include "aio-recv.h"
#include <errno.h>

#define AIO_START_TIMEOUT(aio, timeout, callback)	\
	if (timeout > 0) {								\
		aio_timeout_start(&aio->timeout, timeout, callback, aio); \
	} else {}

#define AIO_STOP_TIMEOUT_ON_FAILED(aio, r, timeout)	\
	if (0 != r && timeout > 0) {					\
		aio_timeout_stop(&aio->timeout);			\
	} else {}

static void aio_timeout_tcp(void* param)
{
	struct aio_recv_t* recv;
	recv = (struct aio_recv_t*)param;

	if (recv->u.onrecv)
		recv->u.onrecv(recv->param, ETIMEDOUT, 0);
}

static void aio_timeout_udp(void* param)
{
	struct aio_recv_t* recv;
	recv = (struct aio_recv_t*)param;

	if (recv->u.onrecvfrom)
		recv->u.onrecvfrom(recv->param, ETIMEDOUT, 0, 0, 0);
}

static void aio_handler_tcp(void* param, int code, size_t bytes)
{
	struct aio_recv_t* recv;
	recv = (struct aio_recv_t*)param;
	if (0 != aio_timeout_stop(&recv->timeout))
		return;

	if (recv->u.onrecv)
		recv->u.onrecv(recv->param, code, bytes);
}

static void aio_handler_udp(void* param, int code, size_t bytes, const struct sockaddr* addr, socklen_t addrlen)
{
	struct aio_recv_t* recv;
	recv = (struct aio_recv_t*)param;
	if (0 != aio_timeout_stop(&recv->timeout))
		return;

	if (recv->u.onrecvfrom)
		recv->u.onrecvfrom(recv->param, code, bytes, addr, addrlen);
}

int aio_recv(struct aio_recv_t* recv, int timeout, aio_socket_t aio, void* buffer, size_t bytes, aio_onrecv onrecv, void* param)
{
	int r;
	recv->param = param;
	recv->u.onrecv = onrecv;
	memset(&recv->timeout, 0, sizeof(recv->timeout));
	AIO_START_TIMEOUT(recv, timeout, aio_timeout_tcp);
	r = aio_socket_recv(aio, buffer, bytes, aio_handler_tcp, recv);
	AIO_STOP_TIMEOUT_ON_FAILED(recv, r, timeout);
	return r;
}

int aio_recv_v(struct aio_recv_t* recv, int timeout, aio_socket_t aio, socket_bufvec_t* vec, int n, aio_onrecv onrecv, void* param)
{
	int r;
	recv->param = param;
	recv->u.onrecv = onrecv;
	memset(&recv->timeout, 0, sizeof(recv->timeout));
	AIO_START_TIMEOUT(recv, timeout, aio_timeout_tcp);
	r = aio_socket_recv_v(aio, vec, n, aio_handler_tcp, recv);
	AIO_STOP_TIMEOUT_ON_FAILED(recv, r, timeout);
	return r;
}

int aio_recvfrom(struct aio_recv_t* recv, int timeout, aio_socket_t aio, void* buffer, size_t bytes, aio_onrecvfrom onrecv, void* param)
{
	int r;
	recv->param = param;
	recv->u.onrecvfrom = onrecv;
	memset(&recv->timeout, 0, sizeof(recv->timeout));
	AIO_START_TIMEOUT(recv, timeout, aio_timeout_udp);
	r = aio_socket_recvfrom(aio, buffer, bytes, aio_handler_udp, recv);
	AIO_STOP_TIMEOUT_ON_FAILED(recv, r, timeout);
	return r;
}

int aio_recvfrom_v(struct aio_recv_t* recv, int timeout, aio_socket_t aio, socket_bufvec_t* vec, int n, aio_onrecvfrom onrecv, void* param)
{
	int r;
	recv->param = param;
	recv->u.onrecvfrom = onrecv;
	memset(&recv->timeout, 0, sizeof(recv->timeout));
	AIO_START_TIMEOUT(recv, timeout, aio_timeout_udp);
	r = aio_socket_recvfrom_v(aio, vec, n, aio_handler_udp, recv);
	AIO_STOP_TIMEOUT_ON_FAILED(recv, r, timeout);
	return r;
}
