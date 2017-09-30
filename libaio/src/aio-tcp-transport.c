#include "aio-tcp-transport.h"
#include "aio-recv.h"
#include "aio-send.h"
#include "aio-rwutil.h"
#include "sys/atomic.h"
#include "sys/spinlock.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TIMEOUT_RECV (4 * 60 * 1000) // 4min
#define TIMEOUT_SEND (2 * 60 * 1000) // 2min

struct aio_tcp_transport_t
{
	spinlock_t locker;
	aio_socket_t socket;
	
	int rtimeout;
	int wtimeout;
	struct aio_recv_t recv;
	struct aio_socket_rw_t send;

	struct aio_tcp_transport_handler_t handler;
	void* param;
};

static void aio_tcp_transport_ondestroy(void* param);
static void aio_tcp_transport_onrecv(void* param, int code, size_t bytes);
static void aio_tcp_transport_onsend(void* param, int code, size_t bytes);

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
	t = (struct aio_tcp_transport_t*)malloc(sizeof(*t));
	if (!t) return NULL;

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
	return aio_socket_destroy(socket, aio_tcp_transport_ondestroy, t);
}

int aio_tcp_transport_send(struct aio_tcp_transport_t* t, const void* data, size_t bytes)
{
	int r = -1;
	spinlock_lock(&t->locker);
	if (invalid_aio_socket != t->socket)
		r = aio_socket_send_all(&t->send, t->wtimeout, t->socket, data, bytes, aio_tcp_transport_onsend, t);
	spinlock_unlock(&t->locker);
	return r;
}

int aio_tcp_transport_send_v(struct aio_tcp_transport_t* t, socket_bufvec_t *vec, int n)
{
	int r = -1;
	spinlock_lock(&t->locker);
	if (invalid_aio_socket != t->socket)
		r = aio_socket_send_v_all(&t->send, t->wtimeout, t->socket, vec, n, aio_tcp_transport_onsend, t);
	spinlock_unlock(&t->locker);
	return r;
}

int aio_tcp_transport_recv(struct aio_tcp_transport_t* t, void* data, size_t bytes)
{
	int r = -1;
	spinlock_lock(&t->locker);
	if(invalid_aio_socket != t->socket)
		r = aio_recv(&t->recv, t->rtimeout, t->socket, data, bytes, aio_tcp_transport_onrecv, t);
	spinlock_unlock(&t->locker);
	return r;
}

int aio_tcp_transport_recv_v(struct aio_tcp_transport_t* t, socket_bufvec_t *vec, int n)
{
	int r = -1;
	spinlock_lock(&t->locker);
	if (invalid_aio_socket != t->socket)
		r = aio_recv_v(&t->recv, t->rtimeout, t->socket, vec, n, aio_tcp_transport_onrecv, t);
	spinlock_unlock(&t->locker);
	return r;
}

void aio_tcp_transport_get_timeout(struct aio_tcp_transport_t* t, int *recvMS, int *sendMS)
{
	if(sendMS) *sendMS = t->wtimeout;
	if(recvMS) *recvMS = t->rtimeout;
}

void aio_tcp_transport_set_timeout(struct aio_tcp_transport_t* t, int recvMS, int sendMS)
{
	recvMS = recvMS < 100 ? 100 : recvMS;
	recvMS = recvMS > 2 * 3600 * 1000 ? 2 * 3600 * 1000 : recvMS;
	sendMS = sendMS < 100 ? 100 : sendMS;
	sendMS = sendMS > 2 * 3600 * 1000 ? 2 * 3600 * 1000 : sendMS;
	t->rtimeout = recvMS;
	t->wtimeout = sendMS;
}

static void aio_tcp_transport_onrecv(void* param, int code, size_t bytes)
{
	struct aio_tcp_transport_t* t;
	t = (struct aio_tcp_transport_t*)param;
	// enable bytes = 0 callback to notify socket close
	t->handler.onrecv(t->param, code, bytes);
}

static void aio_tcp_transport_onsend(void* param, int code, size_t bytes)
{
	struct aio_tcp_transport_t* t;
	t = (struct aio_tcp_transport_t*)param;
	t->handler.onsend(t->param, code, bytes);
}

static void aio_tcp_transport_ondestroy(void* param)
{
	struct aio_tcp_transport_t* t;
	t = (struct aio_tcp_transport_t*)param;
	assert(invalid_aio_socket == t->socket);

	if (t->handler.ondestroy)
		t->handler.ondestroy(t->param);

	spinlock_destroy(&t->locker);
#if defined(DEBUG) || defined(_DEBUG)
	memset(t, 0xCC, sizeof(*t));
#endif
	free(t);
}
