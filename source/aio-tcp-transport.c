#include "aio-tcp-transport.h"
#include "aio-rwutil.h"
#include "sys/locker.h"
#include "sys/atomic.h"
#include "time64.h"
#include "list.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TIMEOUT (4 * 60 * 1000) // 4min

struct aio_connection_list_t
{
	struct list_head root;
	locker_t locker;
};

struct aio_connection_t
{
	int timeout;
	locker_t locker;
	time64_t active;
	aio_socket_t* socket;
	
	struct aio_socket_rw_t write;
	struct list_head node;

	char buffer[2*1024];

	struct aio_tcp_transport_handler_t handler;
	void* param;
};

static struct aio_connection_list_t s_list;

static int aio_tcp_transport_recv(struct aio_connection_t* conn);
static int aio_tcp_transport_close(struct aio_connection_t* conn);
static void aio_tcp_transport_onrecv(void* param, int code, size_t bytes);
static void aio_tcp_transport_onsend(void* param, int code, size_t bytes);
static void aio_tcp_transport_ondestroy(void* param);

void aio_tcp_transport_init(void)
{
	LIST_INIT_HEAD(&s_list.root);
	locker_create(&s_list.locker);
}

void aio_tcp_transport_clean(void)
{
	locker_destroy(&s_list.locker);
}

void aio_tcp_transport_recycle(void)
{
	time64_t clock;
	struct list_head *p;
	struct aio_connection_t *conn;

	clock = time64_now();
	
	do
	{
		conn = NULL;
		locker_lock(&s_list.locker);
		list_for_each(p, &s_list.root)
		{
			conn = list_entry(p, struct aio_connection_t, node);
			if (0 == conn->active || clock - conn->active > conn->timeout)
			{
				list_remove(&conn->node);
				break; // find timeout item
			}
		}
		locker_unlock(&s_list.locker);

		if (p != &s_list.root && conn)
		{
			aio_tcp_transport_close(conn);
		}
	} while (p != &s_list.root);
}

static void aio_tcp_transport_link(struct aio_connection_t *conn)
{
	locker_lock(&s_list.locker);
	list_insert_after(&s_list.root, &conn->node);
	locker_unlock(&s_list.locker);
}

void* aio_tcp_transport_create(socket_t socket, struct aio_tcp_transport_handler_t *handler, void* param)
{
	struct aio_connection_t* conn;
	conn = (struct aio_connection_t*)calloc(1, sizeof(*conn));
	if (!conn) return NULL;

	LIST_INIT_HEAD(&conn->node);
	conn->socket = aio_socket_create(socket, 1);
	conn->active = time64_now();
	conn->param = param;
	conn->timeout = TIMEOUT;
	memcpy(&conn->handler, handler, sizeof(conn->handler));
	locker_create(&conn->locker);
	aio_tcp_transport_link(conn);

	if (0 != aio_tcp_transport_recv(conn))
	{
		memcpy(&conn->handler, NULL, sizeof(conn->handler));
		aio_tcp_transport_destroy(conn);
		return NULL;
	}
	return conn;
}

int aio_tcp_transport_destroy(void* transport)
{
	struct list_head *p;
	struct aio_connection_t* conn;

	conn = NULL;
	locker_lock(&s_list.locker);
	list_for_each(p, &s_list.root)
	{
		conn = list_entry(p, struct aio_connection_t, node);
		if (conn == transport)
		{
			list_remove(&conn->node);
			break;
		}
	}
	locker_unlock(&s_list.locker);

	if (p != &s_list.root && conn)
	{
		return aio_tcp_transport_close(conn);
	}
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
	if (0 != r) conn->active = 0; // recycle flag
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
	if (0 != r) conn->active = 0; // recycle flag
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
	if (0 != r) conn->active = 0; // recycle flag
	locker_unlock(&conn->locker);
	return r;
}

int aio_tcp_transport_get_timeout(void* transport)
{
	struct aio_connection_t* conn;
	conn = (struct aio_connection_t*)transport;
	return conn->timeout;
}

void aio_tcp_transport_set_timeout(void* transport, int timeout)
{
	struct aio_connection_t* conn;
	conn = (struct aio_connection_t*)transport;
	timeout = timeout < 1000 ? 1000 : timeout;
	conn->timeout = timeout;
}

static void aio_tcp_transport_onrecv(void* param, int code, size_t bytes)
{
	struct aio_connection_t* conn;
	conn = (struct aio_connection_t*)param;

	if (0 == code)
	{
		conn->handler.onrecv(conn->param, conn->buffer, bytes);
	}

	if (0 == code && 0 != bytes)
	{
		// read more data
		aio_tcp_transport_recv(conn);
	}
	else
	{
		conn->active = 0; // recycle flag
	}
}

static void aio_tcp_transport_onsend(void* param, int code, size_t bytes)
{
	struct aio_connection_t* conn;
	conn = (struct aio_connection_t*)param;
	conn->active = code ? 0 : time64_now();
	conn->handler.onsend(conn->param, code, bytes);
}

static void aio_tcp_transport_ondestroy(void* param)
{
	struct aio_connection_t* conn;
	conn = (struct aio_connection_t*)param;
	assert(invalid_aio_socket == conn->socket);

	if (conn->handler.ondestroy)
		conn->handler.ondestroy(conn->param);

	locker_destroy(&conn->locker);
#if defined(DEBUG) || defined(_DEBUG)
	memset(conn, 0xCC, sizeof(*conn));
#endif
	free(conn);
}

static int aio_tcp_transport_close(struct aio_connection_t* conn)
{
	aio_socket_t socket;
	socket = conn->socket;
	locker_lock(&conn->locker);
	conn->socket = invalid_aio_socket;
	locker_unlock(&conn->locker);

	assert(invalid_aio_socket != socket);
	return aio_socket_destroy(socket, aio_tcp_transport_ondestroy, conn);
}
