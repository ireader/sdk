#include "aio-tcp-transport.h"
#include "aio-recv.h"
#include "aio-send.h"
#include "aio-rwutil.h"
#include "sys/atomic.h"
#include "sys/system.h"
#include "sys/spinlock.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TIMEOUT_RECV (4 * 60 * 1000) // 4min
#define TIMEOUT_SEND (2 * 60 * 1000) // 2min

struct aio_tcp_transport_t
{
	int32_t		 ref;
	spinlock_t   locker;
	aio_socket_t socket;
	
	int rtimeout; // recv timeout
	int wtimeout; // send timeout
	uint64_t wclock; // last sent data clock, for check connection alive
	struct aio_recv_t recv;
	struct aio_socket_rw_t send;

	struct aio_tcp_transport_handler_t handler;
	void* param;
};

#define AIO_TRANSPORT_ADDREF(t)		if(atomic_increment32(&t->ref) < 2) { return -1; }
#define AIO_TRANSPORT_ONFAIL(t, r)	if(0 != r) { aio_tcp_transport_release(t); }

static void aio_socket_onclose(void* param);
static void aio_socket_onrecv(void* param, int code, size_t bytes);
static void aio_socket_onsend(void* param, int code, size_t bytes);
static void aio_tcp_transport_release(struct aio_tcp_transport_t*);

struct aio_tcp_transport_t* aio_tcp_transport_create(socket_t socket, struct aio_tcp_transport_handler_t *handler, void* param)
{
	aio_socket_t aio;
	aio = aio_socket_create(socket, 1);
	if (invalid_aio_socket == aio)
		return NULL;
	return aio_tcp_transport_create2(aio, handler, param);
}

struct aio_tcp_transport_t* aio_tcp_transport_create2(aio_socket_t aio, struct aio_tcp_transport_handler_t *handler, void* param)
{
	struct aio_tcp_transport_t* t;
	t = (struct aio_tcp_transport_t*)calloc(1, sizeof(*t));
	if (!t) return NULL;

	t->ref = 1;
	t->socket = aio;
	t->param = param;
	t->rtimeout = TIMEOUT_RECV;
	t->wtimeout = TIMEOUT_SEND;
	spinlock_create(&t->locker);
	memcpy(&t->handler, handler, sizeof(t->handler));
	return t;
}

int aio_tcp_transport_destroy(struct aio_tcp_transport_t* t)
{
	aio_socket_t socket;
	spinlock_lock(&t->locker);
	socket = t->socket;
	t->socket = invalid_aio_socket;
	spinlock_unlock(&t->locker);

	if (invalid_aio_socket == socket)
		return 0;
	return aio_socket_destroy(socket, aio_socket_onclose, t);
}

static void aio_tcp_transport_release(struct aio_tcp_transport_t* t)
{
	if (0 == atomic_decrement32(&t->ref))
	{
		assert(invalid_aio_socket == t->socket);
		if (t->handler.ondestroy)
			t->handler.ondestroy(t->param);

		spinlock_destroy(&t->locker);
#if defined(DEBUG) || defined(_DEBUG)
		memset(t, 0xCC, sizeof(*t));
#endif
		free(t);
	}
}
int aio_tcp_transport_send(struct aio_tcp_transport_t* t, const void* data, size_t bytes)
{
	int r = -1;
	AIO_TRANSPORT_ADDREF(t);
	spinlock_lock(&t->locker);
	if (invalid_aio_socket != t->socket)
		r = aio_socket_send_all(&t->send, t->wtimeout, t->socket, data, bytes, aio_socket_onsend, t);
	spinlock_unlock(&t->locker);
	AIO_TRANSPORT_ONFAIL(t, r);
	return r;
}

int aio_tcp_transport_send_v(struct aio_tcp_transport_t* t, socket_bufvec_t *vec, int n)
{
	int r = -1;
	AIO_TRANSPORT_ADDREF(t);
	spinlock_lock(&t->locker);
	if (invalid_aio_socket != t->socket)
		r = aio_socket_send_v_all(&t->send, t->wtimeout, t->socket, vec, n, aio_socket_onsend, t);
	spinlock_unlock(&t->locker);
	AIO_TRANSPORT_ONFAIL(t, r);
	return r;
}

int aio_tcp_transport_recv(struct aio_tcp_transport_t* t, void* data, size_t bytes)
{
	int r = -1;
	AIO_TRANSPORT_ADDREF(t);
	spinlock_lock(&t->locker);
	if(invalid_aio_socket != t->socket)
		r = aio_recv(&t->recv, t->rtimeout, t->socket, data, bytes, aio_socket_onrecv, t);
	spinlock_unlock(&t->locker);
	AIO_TRANSPORT_ONFAIL(t, r);
	return r;
}

int aio_tcp_transport_recv_v(struct aio_tcp_transport_t* t, socket_bufvec_t *vec, int n)
{
	int r = -1;

	AIO_TRANSPORT_ADDREF(t);
	spinlock_lock(&t->locker);
	if (invalid_aio_socket != t->socket)
		r = aio_recv_v(&t->recv, t->rtimeout, t->socket, vec, n, aio_socket_onrecv, t);
	spinlock_unlock(&t->locker);
	AIO_TRANSPORT_ONFAIL(t, r);
	return r;
}

void aio_tcp_transport_get_timeout(struct aio_tcp_transport_t* t, int *recvMS, int *sendMS)
{
	if(sendMS) *sendMS = t->wtimeout;
	if(recvMS) *recvMS = t->rtimeout;
}

void aio_tcp_transport_set_timeout(struct aio_tcp_transport_t* t, int recvMS, int sendMS)
{
	recvMS = recvMS > 0 ? (recvMS < 100 ? 100 : recvMS) : 0;
	recvMS = recvMS > 2 * 3600 * 1000 ? 2 * 3600 * 1000 : recvMS;
	sendMS = sendMS > 0 ? (sendMS < 100 ? 100 : sendMS) : 0;
	sendMS = sendMS > 2 * 3600 * 1000 ? 2 * 3600 * 1000 : sendMS;
	t->rtimeout = recvMS;
	t->wtimeout = sendMS;
}

static void aio_socket_onrecv(void* param, int code, size_t bytes)
{
	struct aio_tcp_transport_t* t;
	t = (struct aio_tcp_transport_t*)param;

	if (ETIMEDOUT == code 
		&& t->wclock + t->rtimeout > system_clock() 
		&& 0 == aio_recv_retry(&t->recv, t->rtimeout))
	{
		// if we have active send connection, recv timeout maybe normal case
		// e.g. RTMP play session
		return;
	}

	// enable bytes = 0 callback to notify socket close
	t->handler.onrecv(t->param, code, bytes);
	aio_tcp_transport_release(t);
}

static void aio_socket_onsend(void* param, int code, size_t bytes)
{
	struct aio_tcp_transport_t* t;
	t = (struct aio_tcp_transport_t*)param;
	t->wclock = system_clock();
	t->handler.onsend(t->param, code, bytes);
	aio_tcp_transport_release(t);
}

static void aio_socket_onclose(void* param)
{
	struct aio_tcp_transport_t* t;
	t = (struct aio_tcp_transport_t*)param;
	aio_tcp_transport_release(t);
}

