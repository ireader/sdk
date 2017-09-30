#include "aio-connect.h"
#include "aio-timeout.h"
#include "aio-socket.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "sockutil.h"
#include <stdlib.h>

struct aio_connect_t
{
	int code;
	u_short port;
	socket_t socket;
	aio_socket_t aio;
	struct addrinfo *addr, *ptr;
	struct aio_timeout_t timer;
	int timeout;

	void (*onconnect)(void* param, aio_socket_t aio, int code);
	void* param;
};

static void aio_connect_ontimeout(void* param);
static void aio_connect_destroy(struct aio_connect_t* conn, int code);
static void aio_connect_addr(struct aio_connect_t* conn);
static void aio_connect_onconnect(void* param, int code);

static void aio_connect_destroy(struct aio_connect_t* conn, int code)
{
	conn->onconnect(conn->param, conn->aio, code);

	if (conn->addr)
		freeaddrinfo(conn->addr);
	free(conn);
}

static void aio_connect_ondestroy(void* param)
{
	struct aio_connect_t* conn;
	conn = (struct aio_connect_t*)param;
	aio_connect_addr(conn); // try next addr
}

static void aio_connect_ontimeout(void* param)
{
	struct aio_connect_t* conn;
	conn = (struct aio_connect_t*)param;
	aio_socket_destroy(conn->aio, aio_connect_ondestroy, conn); // cancel
	conn->aio = NULL;
}

static void aio_connect_onconnect(void* param, int code)
{
	struct aio_connect_t* conn;
	conn = (struct aio_connect_t*)param;
	if (0 != aio_timeout_stop(&conn->timer))
		return;

	conn->code = code;
	aio_connect_destroy(conn, 0);
}

static void aio_connect_addr(struct aio_connect_t* conn)
{
	int r;
	struct addrinfo *addr;

	r = conn->code;
	for(addr = conn->ptr; NULL != addr; addr = conn->ptr)
	{
		conn->ptr = conn->ptr->ai_next;
		conn->socket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (socket_invalid == conn->socket)
			continue;

		// fixed ios getaddrinfo don't set port if nodename is ipv4 address
		socket_addr_setport(addr->ai_addr, addr->ai_addrlen, conn->port);

#if defined(OS_WINDOWS)
		socket_bind_any(conn->socket, 0);
#endif
		//socket_setnonblock(conn->socket, 1);
		//socket_setnondelay(conn->socket, 1);
		conn->aio = aio_socket_create(conn->socket, 1);

		// TODO: lock
		r = aio_timeout_start(&conn->timer, conn->timeout, aio_connect_ontimeout, conn);
		r = aio_socket_connect(conn->aio, addr->ai_addr, addr->ai_addrlen, aio_connect_onconnect, conn);
		if (0 == r)
			return;

		aio_socket_destroy(conn->aio, NULL, NULL); // try next addr
	}

	aio_connect_destroy(conn, r);
}

int aio_connect(const char* host, int port, int timeout, void (*onconnect)(void* param, aio_socket_t aio, int code), void* param)
{
	int r;
	char portstr[16];
	struct aio_connect_t* conn;
	struct addrinfo hints, *addr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
//	hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
	snprintf(portstr, sizeof(portstr), "%hu", port);
	r = getaddrinfo(host, portstr, &hints, &addr);
	if (0 != r)
		return r;

	conn = calloc(1, sizeof(*conn));
	conn->onconnect = onconnect;
	conn->param = param;
	conn->addr = addr;
	conn->ptr = addr;
	conn->port = (u_short)port;
	conn->code = -1;
	conn->timeout = timeout;
	aio_connect_addr(conn);
	return 0;
}
