#include "aio-tcp-transport.h"
#include "aio-rwutil.h"
#include "sys/locker.h"
#include "sys/atomic.h"
#include "time64.h"
#include "list.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct aio_transport_list_t
{
	struct list_head root;
	locker_t locker;
	int timeout;
};

struct aio_connection_t
{
	locker_t locker;
	time64_t active;
	aio_socket_t* socket;
	
	struct aio_socket_rw_t write;
	struct list_head node;

	char buffer[2*1024];

	struct aio_tcp_transport_handler_t handler;
	void* param;
};

static struct aio_transport_list_t s_list;

static int aio_tcp_transport_start(struct aio_connection_t* conn);
static int aio_tcp_transport_recv(struct aio_connection_t* conn);
static int aio_tcp_transport_destroy(struct aio_connection_t* conn);
static void aio_tcp_transport_onrecv(void* param, int code, size_t bytes);
static void aio_tcp_transport_onsend(void* param, int code, size_t bytes);
static void aio_tcp_transport_ondestroy(void* param);

void aio_tcp_transport_init(void)
{
	LIST_INIT_HEAD(&s_list.root);
	locker_create(&s_list.locker);
	s_list.timeout = 4 * 60 * 1000; // 4min
}

void aio_tcp_transport_clean(void)
{
	locker_destroy(&s_list.locker);
}

int aio_tcp_transport_get_timeout(void)
{
	return s_list.timeout;
}

void aio_tcp_transport_set_timeout(int timeout)
{
	if (timeout > 1000)
		s_list.timeout = timeout;
}

void aio_tcp_transport_recycle(void)
{
	time64_t clock;
	struct list_head root;
	struct list_head *p, *next;
	struct aio_connection_t *conn;

	clock = time64_now();
	LIST_INIT_HEAD(&root);
	locker_lock(&s_list.locker);
	list_for_each_safe(p, next, &s_list.root)
	{
		conn = list_entry(p, struct aio_connection_t, node);
		if (clock - conn->active > s_list.timeout)
		{
			list_remove(&conn->node);
			list_insert_after(&conn->node, &root);
		}
	}
	locker_unlock(&s_list.locker);

	list_for_each_safe(p, next, &root)
	{
		conn = list_entry(p, struct aio_connection_t, node);
		aio_tcp_transport_destroy(conn);
	}
}

static void aio_tcp_transport_link(struct aio_connection_t *conn)
{
	locker_lock(&s_list.locker);
	list_insert_after(&s_list.root, &conn->node);
	locker_unlock(&s_list.locker);
}

int aio_tcp_transport_create(socket_t socket, struct aio_tcp_transport_handler_t *handler, void* param)
{
	struct aio_connection_t* conn;
	conn = (struct aio_connection_t*)calloc(1, sizeof(*conn));
	if (!conn) return -1;

	LIST_INIT_HEAD(&conn->node);
	conn->socket = aio_socket_create(socket, 1);
	conn->active = time64_now();
	conn->param = param;
	memcpy(&conn->handler, handler, sizeof(conn->handler));
	locker_create(&conn->locker);
	aio_tcp_transport_link(conn);
	aio_tcp_transport_start(conn);
	return 0;
}

int aio_tcp_transport_send(void* transport, const void* data, size_t bytes)
{
	int r = -1;
	struct aio_connection_t* conn;
	conn = (struct aio_connection_t*)transport;

	locker_lock(&conn->locker);
	conn->active = time64_now();
	if (invalid_aio_socket != conn->socket)
		r = aio_socket_send_all(&conn->write, conn->socket, data, bytes, aio_tcp_transport_onsend, conn);
	locker_unlock(&conn->locker);
	return r;
}

int aio_tcp_transport_sendv(void* transport, socket_bufvec_t *vec, int n)
{
	int r = -1;
	struct aio_connection_t* conn;
	conn = (struct aio_connection_t*)transport;

	locker_lock(&conn->locker);
	conn->active = time64_now();
	if (invalid_aio_socket != conn->socket)
		r = aio_socket_send_v_all(&conn->write, conn->socket, vec, n, aio_tcp_transport_onsend, conn);
	locker_unlock(&conn->locker);
	return r;
}

static int aio_tcp_transport_recv(struct aio_connection_t* conn)
{
	int r = -1;
	locker_lock(&conn->locker);
	conn->active = time64_now();
	if(invalid_aio_socket != conn->socket)
		r = aio_socket_recv(conn->socket, conn->buffer, sizeof(conn->buffer), aio_tcp_transport_onrecv, conn);
	locker_unlock(&conn->locker);
	return r;
}

static int aio_tcp_transport_start(struct aio_connection_t* conn)
{
	if (conn->handler.oncreate)
		conn->handler.oncreate(conn->param, conn);

	return aio_tcp_transport_recv(conn);
}

static void aio_tcp_transport_onrecv(void* param, int code, size_t bytes)
{
	struct aio_connection_t* conn;
	conn = (struct aio_connection_t*)param;

	if (0 == code && 0 != bytes)
	{
		conn->handler.onrecv(conn->param, conn, conn->buffer, bytes);

		// read more data
		code = aio_tcp_transport_recv(conn);
	}

	if (0 != code || 0 == bytes)
	{
		conn->active = 0; // recycle flag
//		aio_tcp_transport_destroy(conn);
	}
}

static void aio_tcp_transport_onsend(void* param, int code, size_t bytes)
{
	struct aio_connection_t* conn;
	conn = (struct aio_connection_t*)param;
	conn->handler.onsend(conn->param, conn, code, bytes);
	conn->active = code ? 0 : time64_now();
	(void)bytes;
}

static void aio_tcp_transport_ondestroy(void* param)
{
	struct aio_connection_t* conn;
	conn = (struct aio_connection_t*)param;
	assert(invalid_aio_socket == conn->socket);

	if (conn->handler.ondestroy)
		conn->handler.ondestroy(conn->param, conn);

	locker_destroy(&conn->locker);
#if defined(DEBUG) || defined(_DEBUG)
	memset(conn, 0xCC, sizeof(*conn));
#endif
	free(conn);
}

static int aio_tcp_transport_destroy(struct aio_connection_t* conn)
{
	aio_socket_t socket;
	socket = conn->socket;
	locker_lock(&conn->locker);
	conn->socket = invalid_aio_socket;
	locker_unlock(&conn->locker);

	return aio_socket_destroy(socket, aio_tcp_transport_ondestroy, conn);
}
