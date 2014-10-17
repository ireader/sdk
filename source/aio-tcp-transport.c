#include "cstringext.h"
#include "aio-socket.h"
#include "aio-tcp-transport.h"
#include "sys/atomic.h"
#include "sys/spinlock.h"
#include "time64.h"
#include "list.h"

#if defined(_DEBUG) || defined(DEBUG)
#define CHECK_MODE
#endif

#define MAX_TCP_PACKET 1024 // 1KB

struct aio_tcp_session_t
{
	spinlock_t locker;
	volatile int closed; // closed thread id
	volatile int used; // used thread id

	volatile int32_t ref;
	volatile aio_socket_t socket;
	char ip[32]; // IPv4/IPv6
	int port;

	char msg[MAX_TCP_PACKET];
	struct aio_tcp_transport_t *transport;

	void* data; // user attach data
	time64_t active; // TODO: recycle idle session	

#if defined(CHECK_MODE)
	struct list_head node;
#endif
};

struct aio_tcp_transport_t
{
	spinlock_t locker;
	volatile int closed; // closed thread id
	volatile int used; // used thread id

	volatile int32_t ref;
	volatile aio_socket_t socket;

	struct aio_tcp_transport_handler_t handler;
	void* ptr;

#if defined(CHECK_MODE)
	int32_t num;
	struct list_head head;
#endif
};

static int aio_tcp_transport_unlink(struct aio_tcp_transport_t *transport)
{
	if(0 == atomic_decrement32(&transport->ref))
	{
#if defined(CHECK_MODE)
		assert(0 == transport->num);
		assert(list_empty(&transport->head));
#endif
		assert(0 == transport->used);
		if(transport->socket)
		{
			aio_socket_destroy(transport->socket);
			transport->socket = NULL;
		}

		spinlock_destroy(&transport->locker);
#if defined(_DEBUG) || defined(DEBUG)
		memset(transport, 0xCC, sizeof(*transport));
#endif
		free(transport);
	}
	return 0;
}

static struct aio_tcp_session_t* aio_tcp_session_create(struct aio_tcp_transport_t *transport, socket_t socket, const char* ip, int port)
{
	struct aio_tcp_session_t *session;

	session = (struct aio_tcp_session_t*)malloc(sizeof(*session));
	if(session)
	{
		memset(session, 0, sizeof(*session));
		spinlock_create(&session->locker);
		session->ref = 1;
		session->transport = transport;
		session->socket = aio_socket_create(socket, 1);
		strncpy(session->ip, ip, sizeof(session->ip));
		session->port = port;

		atomic_increment32(&transport->ref);

#if defined(CHECK_MODE)
		atomic_increment32(&transport->num);
		spinlock_lock(&transport->locker);
		list_insert_after(&session->node, transport->head.prev);
		spinlock_unlock(&transport->locker);
#endif
	}

	return session;
}

static void aio_tcp_session_destroy(struct aio_tcp_session_t *session)
{
	struct aio_tcp_transport_t *transport;
	transport = session->transport;

#if defined(CHECK_MODE)
	spinlock_lock(&transport->locker);
	list_remove(&session->node);
	spinlock_unlock(&transport->locker);
	atomic_decrement32(&transport->num);
#endif

	aio_tcp_transport_unlink(transport);

	assert(0 == session->used);
	spinlock_destroy(&session->locker);
	assert(0 == session->ref);
#if defined(DEBUG) || defined(_DEBUG)
	memset(session, 0xCC, sizeof(*session));
#endif

	free(session);
}

static void aio_tcp_session_onrecv(void* param, int code, size_t bytes)
{
	int r = 0;
	int closed = 0;
	struct aio_tcp_session_t *session;
	struct aio_tcp_transport_t *transport;

	session = (struct aio_tcp_session_t *)param;
	transport = session->transport;

	assert(session->ref > 0);
	session->active = time64_now();

	if(0 != code || 0 == bytes)
	{
		// 1. connection error
		// 2. client close connection
		transport->handler.ondisconnected(session->data);
		aio_tcp_transport_release(session);
	}
	else
	{
		assert(0 != bytes);
		transport->handler.onrecv(session->data, session->msg, bytes);

		// check status
		spinlock_lock(&session->locker);
		closed = session->closed;
		if(0 == closed) ++session->used;
		spinlock_unlock(&session->locker);

		if(0 == closed)
		{
			// receive more data
			r = aio_socket_recv(session->socket, session->msg, sizeof(session->msg), aio_tcp_session_onrecv, session);
			if(0 != r)
				transport->handler.ondisconnected(session->data);
		}

		spinlock_lock(&session->locker);
		if(0 == closed) --session->used;
		spinlock_unlock(&session->locker);

		if(1 == closed || 0 != r)
			aio_tcp_transport_release(session);
	}
}

static void aio_tcp_session_onsend(void* param, int code, size_t bytes)
{
	struct aio_tcp_session_t *session;
	struct aio_tcp_transport_t *transport;
	session = (struct aio_tcp_session_t *)param;
	transport = session->transport;
	transport->handler.onsend(session->data, code, bytes);
}

static void aio_tcp_transport_onaccept(void* param, int code, socket_t socket, const char* ip, int port)
{
	int r = 0;
	int closed = 0;
	struct aio_tcp_session_t *session;
	struct aio_tcp_transport_t *transport;
	transport = (struct aio_tcp_transport_t*)param;

	if(0 != code)
	{
		assert(0);
		printf("aio_tcp_transport_onaccept => %d\n", code);
		aio_tcp_transport_unlink(transport);
		return;
	}

	spinlock_lock(&transport->locker);
	assert(transport->used >= 0);
	if(0 == transport->closed)
		transport->used += 1;
	closed = transport->closed;
	spinlock_unlock(&transport->locker);

	if(0 == closed)
	{
		// accept another connection
		r = aio_socket_accept(transport->socket, aio_tcp_transport_onaccept, transport);
	}

	if(0 == code)
	{
		session = aio_tcp_session_create(transport, socket, ip, port);
		if(!session)
		{
			assert(0);
		}
		else
		{
			atomic_increment32(&transport->ref);
			session->data = transport->handler.onconnected(transport->ptr, session, session->ip, session->port);

			if(0 != aio_socket_recv(session->socket, session->msg, sizeof(session->msg), aio_tcp_session_onrecv, session))
			{
				printf("aio_tcp_transport_onaccept aio_socket_recv failed.\n");
				transport->handler.ondisconnected(session->data);
				aio_tcp_transport_release(session);
			}

			// link session
		}
	}

	spinlock_lock(&transport->locker);
	if(0 == closed)
		--transport->used;
	assert(transport->used >= 0);
	spinlock_unlock(&transport->locker);

	if(1 == closed || 0 != r)
		aio_tcp_transport_unlink(transport);
}

void* aio_tcp_transport_create(socket_t socket, const struct aio_tcp_transport_handler_t *handler, void* ptr)
{
	struct aio_tcp_transport_t *transport;
	transport = (struct aio_tcp_transport_t*)malloc(sizeof(*transport));
	if(!transport)
		return NULL;

	memset(transport, 0, sizeof(*transport));
	spinlock_create(&transport->locker);
	transport->ref = 1;
#if defined(CHECK_MODE)
	transport->num = 0;
	LIST_INIT_HEAD(&transport->head);
#endif
	transport->socket = aio_socket_create(socket, 1);
	if(invalid_aio_socket == transport->socket)
	{
		aio_tcp_transport_destroy(transport);
		return NULL;
	}

	++transport->ref;
	if(0 != aio_socket_accept(transport->socket, aio_tcp_transport_onaccept, transport))
	{
		printf("aio_tcp_transport_create aio accept error.\n");
		--transport->ref;
		aio_tcp_transport_destroy(transport);
		return NULL;
	}

	memcpy(&transport->handler, handler, sizeof(transport->handler));
	transport->ptr = ptr;
	return transport;
}

int aio_tcp_transport_destroy(void* t)
{
	struct aio_tcp_transport_t *transport;
	transport = (struct aio_tcp_transport_t *)t;

	spinlock_lock(&transport->locker);
	assert(transport->socket);
	transport->closed = 1; // mark closed by user
	if(0 == transport->used)
	{
		aio_socket_destroy(transport->socket);
		transport->socket = NULL;
	}
	spinlock_unlock(&transport->locker);

	return aio_tcp_transport_unlink(transport);
}

int aio_tcp_transport_disconnect(void* s)
{
	struct aio_tcp_session_t *session;
	struct aio_tcp_transport_t *transport;
	session = (struct aio_tcp_session_t *)s;
	transport = session->transport;

	assert(session->ref > 0);
	if(session->socket)
	{
		aio_socket_destroy(session->socket);
		session->socket = NULL;
	}
	return 0;
}

int aio_tcp_transport_send(void* s, const void* msg, size_t bytes)
{
	struct aio_tcp_session_t *session;
	session = (struct aio_tcp_session_t *)s;
	session->active = time64_now();
	assert(session->ref > 0 && session->socket);
	return aio_socket_send(session->socket, msg, bytes, aio_tcp_session_onsend, session);
}

int aio_tcp_transport_sendv(void* s, socket_bufvec_t *vec, int n)
{
	struct aio_tcp_session_t *session;
	session = (struct aio_tcp_session_t *)s;

	session->active = time64_now();
	assert(session->ref > 0 && session->socket);
	return aio_socket_send_v(session->socket, vec, n, aio_tcp_session_onsend, session);
}

int aio_tcp_transport_addref(void* s)
{
	struct aio_tcp_session_t *session;
	session = (struct aio_tcp_session_t *)s;
	assert(session->ref > 0);
	return InterlockedIncrement(&session->ref);
}

int aio_tcp_transport_release(void* s)
{
	struct aio_tcp_session_t *session;
	session = (struct aio_tcp_session_t *)s;

	assert(session->ref > 0);
	if(0 == InterlockedDecrement(&session->ref))
	{
		aio_tcp_session_destroy(session);
	}

	return 0;
}
