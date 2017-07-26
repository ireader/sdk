#include "aio-tcp-transport.h"
#include "aio-timeout.h"
#include "aio-rwutil.h"
#include "sys/atomic.h"
#include "sys/spinlock.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TIMEOUT (4 * 60 * 1000) // 4min

struct aio_tcp_transport_t
{
	spinlock_t locker;
	aio_socket_t* socket;
	
	struct aio_timeout_t timer;
	struct aio_socket_rw_t write;

	char buffer[2*1024];

	struct aio_tcp_transport_handler_t handler;
	void* param;
};

static int aio_tcp_transport_recv(struct aio_tcp_transport_t* t);
static void aio_tcp_transport_onrecv(void* param, int code, size_t bytes);
static void aio_tcp_transport_onsend(void* param, int code, size_t bytes);
static void aio_tcp_transport_ondestroy(void* param);
static void aio_timer_onnotify(void* param);
static void aio_timer_oncancel(void* param);

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

	t->socket = aio;
	t->param = param;
	spinlock_create(&t->locker);
	memcpy(&t->handler, handler, sizeof(t->handler));
	aio_timeout_add(&t->timer, TIMEOUT, aio_timer_onnotify, t);

	if (0 != aio_tcp_transport_recv(t))
	{
		memset(&t->handler, 0, sizeof(t->handler));
		aio_tcp_transport_stop(t);
		return NULL;
	}
	return t;
}

int aio_tcp_transport_stop(struct aio_tcp_transport_t* t)
{
	aio_socket_t socket;
	socket = t->socket;
	spinlock_lock(&t->locker);
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
		r = aio_socket_send_all(&t->write, t->socket, data, bytes, aio_tcp_transport_onsend, t);
	spinlock_unlock(&t->locker);

	if (0 == r) aio_timeout_start(&t->timer);
	return r;
}

int aio_tcp_transport_sendv(struct aio_tcp_transport_t* t, socket_bufvec_t *vec, int n)
{
	int r = -1;
	spinlock_lock(&t->locker);
	if (invalid_aio_socket != t->socket)
		r = aio_socket_send_v_all(&t->write, t->socket, vec, n, aio_tcp_transport_onsend, t);
	spinlock_unlock(&t->locker);

	if(0 == r) aio_timeout_start(&t->timer);
	return r;
}

static int aio_tcp_transport_recv(struct aio_tcp_transport_t* t)
{
	int r = -1;
	spinlock_lock(&t->locker);
	if(invalid_aio_socket != t->socket)
		r = aio_socket_recv(t->socket, t->buffer, sizeof(t->buffer), aio_tcp_transport_onrecv, t);
	spinlock_unlock(&t->locker);

	if (0 == r)
		aio_timeout_start(&t->timer);
	else
		aio_tcp_transport_stop(t);
	return r;
}

int aio_tcp_transport_get_timeout(struct aio_tcp_transport_t* t)
{
	return t->timer.timeout;
}

void aio_tcp_transport_set_timeout(struct aio_tcp_transport_t* t, int timeout)
{
	timeout = timeout < 1000 ? 1000 : timeout;
	aio_timeout_settimeout(&t->timer, timeout);
}

static void aio_tcp_transport_onrecv(void* param, int code, size_t bytes)
{
	struct aio_tcp_transport_t* t;
	t = (struct aio_tcp_transport_t*)param;

	if (0 == code)
	{
		t->handler.onrecv(t->param, t->buffer, bytes);
	}

	if (0 == code && 0 != bytes)
	{
		// read more data
		aio_tcp_transport_recv(t);
	}
	else
	{
		// close aio socket
		aio_tcp_transport_stop(t);
	}
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
	aio_timeout_delete(&t->timer, aio_timer_oncancel, t);
}

static void aio_timer_onnotify(void* param)
{
	struct aio_tcp_transport_t* t;
	t = (struct aio_tcp_transport_t*)param;
	aio_tcp_transport_stop(t);
}

static void aio_timer_oncancel(void* param)
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
