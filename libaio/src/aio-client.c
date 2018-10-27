#include "aio-client.h"
#include "aio-socket.h"
#include "aio-rwutil.h"
#include "aio-connect.h"
#include "aio-recv.h"
#include "sys/atomic.h"
#include "sys/spinlock.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define RECV 0
#define SEND 1

#define TIMEOUT_CONN (2 * 60 * 1000) // 2min
#define TIMEOUT_RECV (4 * 60 * 1000) // 4min
#define TIMEOUT_SEND (2 * 60 * 1000) // 2min

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

enum { RW_NONE = 0, RW_DATA, RW_VECTOR };
enum { AIO_NONE = 0, AIO_CONNECTING, AIO_CONNECTED };

struct aio_client_data_t
{
	int state; // 0-none, 1-send/recv, 2-send/recv vec

	union
	{
		void* data;
		socket_bufvec_t *vec;
	} u;

	size_t count; // data bytes / vec count
};

struct aio_client_t
{
	int32_t ref;
	spinlock_t locker;
	aio_socket_t socket;

	int state; // 0-unconnect, 1-connecting, 2-connected
	char* host;
	int port;
	int ctimeout;
	int rtimeout;
	int wtimeout;

	struct aio_recv_t recv;
	struct aio_socket_rw_t send;
	struct aio_client_data_t data[2]; // 0-recv/1-send

	struct aio_client_handler_t handler;
	void* param;
};

static int aio_client_disconnect_internal(aio_client_t* client);
static int aio_client_connect_internal(aio_client_t* client);
static void aio_client_release(aio_client_t* client);
static void aio_client_ondestroy(void* param);
static void aio_client_onrecv(void* param, int code, size_t bytes);
static void aio_client_onsend(void* param, int code, size_t bytes);
static void aio_client_onconn(void* param, int code, aio_socket_t socket);

aio_client_t* aio_client_create(const char* host, int port, struct aio_client_handler_t* handler, void* param)
{
	size_t len;
	struct aio_client_t* client;

	len = strlen(host ? host : "");
	if (len < 1) return NULL;

	client = (struct aio_client_t*)calloc(1, sizeof(*client) + len + 1);
	if (!client) return NULL;

	client->ref = 1;
	client->port = port;
	client->host = (char*)(client + 1);
	memcpy(client->host, host, len + 1);
	
	spinlock_create(&client->locker);
	client->socket = invalid_aio_socket;
	client->ctimeout = TIMEOUT_CONN;
	client->rtimeout = TIMEOUT_RECV;
	client->wtimeout = TIMEOUT_SEND;
	
	client->param = param;
	memcpy(&client->handler, handler, sizeof(client->handler));
	return client;
}

int aio_client_destroy(aio_client_t* client)
{
	aio_client_disconnect(client);
	aio_client_release(client);
	return 0;
}

int aio_client_connect(aio_client_t* client)
{
	int r = 0;
	spinlock_lock(&client->locker);
	r = aio_client_connect_internal(client);
	spinlock_unlock(&client->locker);
	return r;
}

int aio_client_disconnect(aio_client_t* client)
{
	int r;
	spinlock_lock(&client->locker);
	r = aio_client_disconnect_internal(client);
	spinlock_unlock(&client->locker);
	return r;
}

int aio_client_recv(aio_client_t* client, void* data, size_t bytes)
{
	int r = -1;
	spinlock_lock(&client->locker);
	if (RW_NONE != client->data[RECV].state)
	{
		spinlock_unlock(&client->locker);
		return -1; // previous recv don't finish
	}

	client->data[RECV].state = RW_DATA;
	client->data[RECV].u.data = data;
	client->data[RECV].count = bytes;
	if (invalid_aio_socket != client->socket)
	{
		r = aio_recv(&client->recv, client->rtimeout, client->socket, data, bytes, aio_client_onrecv, client);
		if (0 == r)
		{
			spinlock_unlock(&client->locker);
			return 0;
		}

		client->data[RECV].state = RW_NONE;
		aio_client_disconnect_internal(client);
	}
	else
	{
		r = aio_client_connect_internal(client);
	}

	spinlock_unlock(&client->locker);
	return r;
}

int aio_client_recv_v(aio_client_t* client, socket_bufvec_t *vec, int n)
{
	int r = -1;
	spinlock_lock(&client->locker);
	if (RW_NONE != client->data[RECV].state)
	{
		spinlock_unlock(&client->locker);
		return -1; // previous recv don't finish
	}

	client->data[RECV].state = RW_VECTOR;
	client->data[RECV].u.vec = vec;
	client->data[RECV].count = n;
	if (invalid_aio_socket != client->socket)
	{
		r = aio_recv_v(&client->recv, client->rtimeout, client->socket, vec, n, aio_client_onrecv, client);
		if (0 == r)
		{
			spinlock_unlock(&client->locker);
			return 0;
		}

		client->data[RECV].state = RW_NONE;
		aio_client_disconnect_internal(client);
	}
	else
	{
		r = aio_client_connect_internal(client);
	}

	spinlock_unlock(&client->locker);
	return r;
}

int aio_client_send(aio_client_t* client, const void* data, size_t bytes)
{
	int r = -1;
	spinlock_lock(&client->locker);
	if (RW_NONE != client->data[SEND].state)
	{
		spinlock_unlock(&client->locker);
		return -1; // previous recv don't finish
	}

	client->data[SEND].state = RW_DATA;
	client->data[SEND].u.data = (void*)data;
	client->data[SEND].count = bytes;
	if (invalid_aio_socket != client->socket)
	{
		r = aio_socket_send_all(&client->send, client->wtimeout, client->socket, data, bytes, aio_client_onsend, client);
		if (0 == r)
		{
			spinlock_unlock(&client->locker);
			return 0;
		}

		client->data[SEND].state = RW_NONE;
		aio_client_disconnect_internal(client);
	}
	else
	{
		r = aio_client_connect_internal(client);
	}

	spinlock_unlock(&client->locker);
	return r;
}

int aio_client_send_v(aio_client_t* client, socket_bufvec_t *vec, int n)
{
	int r = -1;
	spinlock_lock(&client->locker);
	if (RW_NONE != client->data[SEND].state)
	{
		spinlock_unlock(&client->locker);
		return -1; // previous recv don't finish
	}

	client->data[SEND].state = RW_VECTOR;
	client->data[SEND].u.vec = vec;
	client->data[SEND].count = n;
	if (invalid_aio_socket != client->socket)
	{
		r = aio_socket_send_v_all(&client->send, client->wtimeout, client->socket, vec, n, aio_client_onsend, client);
		if (0 == r)
		{
			spinlock_unlock(&client->locker);
			return 0;
		}

		client->data[SEND].state = RW_NONE;
		aio_client_disconnect_internal(client);
	}
	else
	{
		r = aio_client_connect_internal(client);
	}

	spinlock_unlock(&client->locker);
	return r;
}

void aio_client_settimeout(aio_client_t* client, int conn, int recv, int send)
{
	conn = conn > 0 ? MIN(2 * 3600 * 1000, MAX(conn, 100)) : 0;
	recv = recv > 0 ? MIN(2 * 3600 * 1000, MAX(recv, 100)) : 0;
	send = send > 0 ? MIN(2 * 3600 * 1000, MAX(send, 100)) : 0;
	client->ctimeout = conn;
	client->rtimeout = recv;
	client->wtimeout = send;
}

void aio_client_gettimeout(aio_client_t* client, int* conn, int* recv, int* send)
{
	if (conn) *conn = client->ctimeout;
	if (recv) *recv = client->rtimeout;
	if (send) *send = client->wtimeout;
}

static void aio_client_onconn(void* param, int code, aio_socket_t aio)
{
	int send = 0, recv = 0;
	struct aio_client_t* client;
	client = (struct aio_client_t*)param;
	if(client->handler.onconn)
		client->handler.onconn(client->param);

	spinlock_lock(&client->locker);
	if (0 == code)
	{
		client->state = AIO_CONNECTED;
		client->socket = aio;

		if (RW_DATA == client->data[RECV].state)
		{
			recv = aio_recv(&client->recv, client->rtimeout, client->socket, client->data[RECV].u.data, client->data[RECV].count, aio_client_onrecv, client);
		}
		else if (RW_VECTOR == client->data[RECV].state)
		{
			recv = aio_recv_v(&client->recv, client->rtimeout, client->socket, client->data[RECV].u.vec, (int)client->data[RECV].count, aio_client_onrecv, client);
		}

		if(0 != recv)
		{
			assert(0);
			client->data[RECV].state = RW_NONE;
		}

		if (RW_DATA == client->data[SEND].state)
		{
			send = aio_socket_send_all(&client->send, client->wtimeout, client->socket, client->data[SEND].u.data, client->data[SEND].count, aio_client_onsend, client);
		}
		else if (RW_VECTOR == client->data[SEND].state)
		{
			send = aio_socket_send_v_all(&client->send, client->wtimeout, client->socket, client->data[SEND].u.vec, (int)client->data[SEND].count, aio_client_onsend, client);
		}

		if (0 != send)
		{
			assert(0);
			client->data[SEND].state = RW_NONE;
		}
	}
	else
	{
		client->state = AIO_NONE;

		if (RW_NONE != client->data[RECV].state)
		{
			client->data[RECV].state = RW_NONE;
			recv = code;
		}
		if (RW_NONE != client->data[SEND].state)
		{
			client->data[SEND].state = RW_NONE;
			send = code;
		}
	}
	spinlock_unlock(&client->locker);

	if (0 != recv)
	{
		client->handler.onrecv(client->param, recv, 0);
	}
	if (0 != send)
	{
		client->handler.onsend(client->param, send, 0);
	}

	// connect failed
	if (0 != code)
	{
		aio_client_release(client);
	}
}

static void aio_client_onrecv(void* param, int code, size_t bytes)
{
	struct aio_client_t* client;
	client = (struct aio_client_t*)param;
	
	spinlock_lock(&client->locker);
	client->data[RECV].state = RW_NONE; // clear recv state
	if (0 != code || 0 == bytes) aio_client_disconnect_internal(client);
	spinlock_unlock(&client->locker);

	// enable bytes = 0 callback to notify socket close
	client->handler.onrecv(client->param, code, bytes);
}

static void aio_client_onsend(void* param, int code, size_t bytes)
{
	struct aio_client_t* client;
	client = (struct aio_client_t*)param;

	spinlock_lock(&client->locker);
	client->data[SEND].state = RW_NONE; // clear send state
	if (0 != code) aio_client_disconnect_internal(client);
	spinlock_unlock(&client->locker);

	client->handler.onsend(client->param, code, bytes);
}

static void aio_client_ondestroy(void* param)
{
	struct aio_client_t* client;
	client = (struct aio_client_t*)param;
	spinlock_lock(&client->locker);
	// do nothing
	spinlock_unlock(&client->locker);
	aio_client_release(client);
}

static int aio_client_connect_internal(aio_client_t* client)
{
	int r;
	if (AIO_NONE != client->state)
		return 0;

	assert(client->ref > 0);
	++client->ref;
	client->state = AIO_CONNECTING;
	assert(invalid_aio_socket == client->socket);
	r = aio_connect(client->host, client->port, client->ctimeout, aio_client_onconn, client);
	if (0 != r)
	{
		client->state = AIO_NONE; // restore connect state
		--client->ref;
	}
	return r;
}

static int aio_client_disconnect_internal(aio_client_t* client)
{
	if (invalid_aio_socket != client->socket)
	{
		aio_socket_destroy(client->socket, aio_client_ondestroy, client);
		client->socket = invalid_aio_socket;
		client->state = AIO_NONE;
	}
	return 0;
}

static void aio_client_release(aio_client_t* client)
{
	if (0 == atomic_decrement32(&client->ref))
	{
		assert(AIO_NONE == client->state);
		assert(invalid_aio_socket == client->socket);
		assert(RW_NONE == client->data[RECV].state);
		assert(RW_NONE == client->data[SEND].state);

		if (client->handler.ondestroy)
			client->handler.ondestroy(client->param);

		spinlock_destroy(&client->locker);
#if defined(DEBUG) || defined(_DEBUG)
		memset(client, 0xCC, sizeof(*client));
#endif
		free(client);
	}
}
