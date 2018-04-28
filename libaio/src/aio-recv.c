#include "aio-recv.h"
#include "sys/atomic.h"
#include "sys/thread.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

enum { AIO_STATUS_INIT = 0, AIO_STATUS_START, AIO_STATUS_TIMEOUT };

#define AIO_RECV_START(recv) {assert(AIO_STATUS_INIT == recv->status);recv->status = AIO_STATUS_START;}

#define AIO_START_TIMEOUT(aio, timeout, callback)	\
	if (timeout > 0) {								\
		aio_timeout_start(&aio->timeout, timeout, callback, aio); \
	} else {}

#define AIO_STOP_TIMEOUT_ON_FAILED(aio, r, timeout)	\
	if (0 != r) aio->status = AIO_STATUS_INIT;		\
	if (0 != r && timeout > 0) {					\
		aio_timeout_stop(&aio->timeout);			\
	} else {}

static void aio_timeout_tcp(void* param)
{
	struct aio_recv_t* recv;
	recv = (struct aio_recv_t*)param;

	if (atomic_cas32(&recv->status, AIO_STATUS_START, AIO_STATUS_TIMEOUT) && recv->u.onrecv)
		recv->u.onrecv(recv->param, ETIMEDOUT, 0);
}

static void aio_timeout_udp(void* param)
{
	struct aio_recv_t* recv;
	recv = (struct aio_recv_t*)param;

	if (atomic_cas32(&recv->status, AIO_STATUS_START, AIO_STATUS_TIMEOUT) && recv->u.onrecvfrom)
		recv->u.onrecvfrom(recv->param, ETIMEDOUT, 0, 0, 0);
}

static int aio_handler_check(struct aio_recv_t* recv)
{
	while (AIO_STATUS_START == atomic_load32(&recv->status))
	{
		// Thread 1 -> timeout, change status to AIO_STATUS_TIMEOUT
		// Thread 1 -> try to reset timer, change status to AIO_STATUS_START
		// Thread 2 -> aio recv callback, try to stop timer (current)
		// Thread 1 -> start timer
		// Thread 2 -> stop timer success
		if (0 != aio_timeout_stop(&recv->timeout))
		{
			// timer stop failed case:
			// 1. another time-out thread working
			// 2. recv try timer don't start, so wait a short time
			thread_yield();
			continue;
		}

		if (atomic_cas32(&recv->status, AIO_STATUS_START, AIO_STATUS_INIT))
			return 1; // ok
	}

	// recv timeout, but don't reset timer
	// Thread 1 -> timeout, change status to AIO_STATUS_TIMEOUT
	// Thread 2 -> aio recv callback, change to AIO_STATUS_int (current)
	// Thread 1 -> reset timer will failed
	if (!atomic_cas32(&recv->status, AIO_STATUS_TIMEOUT, AIO_STATUS_INIT))
	{
		assert(0);
	}

	return 0;
}

static void aio_handler_tcp(void* param, int code, size_t bytes)
{
	struct aio_recv_t* recv;
	recv = (struct aio_recv_t*)param;
	
	if (aio_handler_check(recv) && recv->u.onrecv)
		recv->u.onrecv(recv->param, code, bytes);
}

static void aio_handler_udp(void* param, int code, size_t bytes, const struct sockaddr* addr, socklen_t addrlen)
{
	struct aio_recv_t* recv;
	recv = (struct aio_recv_t*)param;

	if (aio_handler_check(recv) && recv->u.onrecvfrom)
		recv->u.onrecvfrom(recv->param, code, bytes, addr, addrlen);
}

int aio_recv(struct aio_recv_t* recv, int timeout, aio_socket_t aio, void* buffer, size_t bytes, aio_onrecv onrecv, void* param)
{
	int r;
	AIO_RECV_START(recv);
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
	AIO_RECV_START(recv);
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
	AIO_RECV_START(recv);
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
	AIO_RECV_START(recv);
	recv->param = param;
	recv->u.onrecvfrom = onrecv;
	memset(&recv->timeout, 0, sizeof(recv->timeout));
	AIO_START_TIMEOUT(recv, timeout, aio_timeout_udp);
	r = aio_socket_recvfrom_v(aio, vec, n, aio_handler_udp, recv);
	AIO_STOP_TIMEOUT_ON_FAILED(recv, r, timeout);
	return r;
}

int aio_recv_retry(struct aio_recv_t* recv, int timeout)
{
	timeout = timeout < 1 ? 1 : timeout;
	assert(recv->u.onrecv || recv->u.onrecvfrom);
	// Thread 1 -> timeout, change status to AIO_STATUS_TIMEOUT
	// Thread 2 -> aio recv callback, change to AIO_STATUS_int
	// Thread 1 -> reset timer failed (current)
	if (!atomic_cas32(&recv->status, AIO_STATUS_TIMEOUT, AIO_STATUS_START))
		return -1;
	return aio_timeout_start(&recv->timeout, timeout, aio_timeout_tcp, recv);
}

int aio_recvfrom_retry(struct aio_recv_t* recv, int timeout)
{
	timeout = timeout < 1 ? 1 : timeout;
	assert(recv->u.onrecv || recv->u.onrecvfrom);
	if (!atomic_cas32(&recv->status, AIO_STATUS_TIMEOUT, AIO_STATUS_START))
		return -1;
	return aio_timeout_start(&recv->timeout, timeout, aio_timeout_udp, recv);
}
