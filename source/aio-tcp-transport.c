#include "cstringext.h"
#include "aio-socket.h"
#include "aio-tcp-transport.h"
#include "time64.h"

#define MAX_TCP_PACKET 1024 // 1KB

enum { CONNECTED = 1, DISCONNECTED };

struct aio_tcp_session_t
{
	char msg[MAX_TCP_PACKET];

	struct aio_tcp_transport_t *transport;

	aio_socket_t socket;
	char ip[32]; // IPv4/IPv6
	int port;

	int status;
	void* data; // user attach data
	time64_t active; // TODO: recycle idle session
	long ref;
};

struct aio_tcp_transport_t
{
	aio_socket_t socket;

	struct aio_tcp_transport_handler_t handler;
	void* ptr;

#if defined(DEBUG) || defined(_DEBUG)
	long ref;
#endif

	struct aio_tcp_session_t *head;
};

static struct aio_tcp_session_t* aio_tcp_session_create(struct aio_tcp_transport_t *transport)
{
	struct aio_tcp_session_t *session;

	session = (struct aio_tcp_session_t*)malloc(sizeof(*session));
	if(session)
	{
		memset(session, 0, sizeof(*session));
		session->ref = 1;
		session->transport = transport;
#if defined(DEBUG) || defined(_DEBUG)
		InterlockedIncrement(&transport->ref);
#endif
	}

	return session;
}

static void aio_tcp_transport_onrecv(void* param, int code, size_t bytes)
{
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
		session->status = DISCONNECTED;
		transport->handler.ondisconnected(session->data);
		aio_tcp_transport_release(session);
	}
	else
	{
		assert(0 != bytes);
		transport->handler.onrecv(session->data, session->msg, bytes);

		// receive more data
		if(0 != aio_socket_recv(session->socket, session->msg, sizeof(session->msg), aio_tcp_transport_onrecv, session))
		{
			printf("aio_tcp_transport_onrecv aio_socket_recv failed.\n");
			session->status = DISCONNECTED;
			transport->handler.ondisconnected(session->data);
			aio_tcp_transport_release(session);
		}
	}
}

static void aio_tcp_transport_onsend(void* param, int code, size_t bytes)
{
	struct aio_tcp_session_t *session;
	struct aio_tcp_transport_t *transport;
	session = (struct aio_tcp_session_t *)param;
	transport = session->transport;
	transport->handler.onsend(session->data, code, bytes);
}

static void aio_tcp_transport_onaccept(void* param, int code, socket_t socket, const char* ip, int port)
{
	struct aio_tcp_session_t *session;
	struct aio_tcp_transport_t *transport;
	transport = (struct aio_tcp_transport_t*)param;

	if(0 != code)
	{
		assert(0);
		printf("aio_tcp_transport_onaccept => %d\n", code);
	}

	// accept other connection
	if(0 != aio_socket_accept(transport->socket, aio_tcp_transport_onaccept, transport))
	{
		exit(1000);
	}

	if(0 == code)
	{
		session = aio_tcp_session_create(transport);
		if(!session)
		{
			assert(0);
		}
		else
		{
			session->socket = aio_socket_create(socket, 1);
			strncpy(session->ip, ip, sizeof(session->ip));
			session->port = port;

			session->data = transport->handler.onconnected(transport->ptr, session, session->ip, session->port);

			if(0 != aio_socket_recv(session->socket, session->msg, sizeof(session->msg), aio_tcp_transport_onrecv, session))
			{
				printf("aio_tcp_transport_onaccept aio_socket_recv failed.\n");
				session->status = DISCONNECTED;
				transport->handler.ondisconnected(session->data);
				aio_tcp_transport_release(session);
			}

			// link session
		}
	}
}

void* aio_tcp_transport_create(socket_t socket, const struct aio_tcp_transport_handler_t *handler, void* ptr)
{
	struct aio_tcp_transport_t *transport;
	transport = (struct aio_tcp_transport_t*)malloc(sizeof(*transport));
	if(!transport)
		return NULL;

	memset(transport, 0, sizeof(*transport));
	transport->socket = aio_socket_create(socket, 1);
	if(invalid_aio_socket == transport->socket)
	{
		aio_tcp_transport_destroy(transport);
		return NULL;
	}

	if(0 != aio_socket_accept(transport->socket, aio_tcp_transport_onaccept, transport))
	{
		printf("aio_tcp_transport_create aio accept error.\n");
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

	// make sure all session have finished.
	assert(0 == transport->ref);

	if(transport->socket)
		aio_socket_destroy(transport->socket);

#if defined(_DEBUG) || defined(DEBUG)
	memset(transport, 0xCC, sizeof(*transport));
#endif
	free(transport);
	return 0;
}

int aio_tcp_transport_send(void* s, const void* msg, size_t bytes)
{
	struct aio_tcp_session_t *session;
	session = (struct aio_tcp_session_t *)s;

	assert(session->ref > 0);
	session->active = time64_now();
	return aio_socket_send(session->socket, msg, bytes, aio_tcp_transport_onsend, session);
}

int aio_tcp_transport_sendv(void* s, socket_bufvec_t *vec, int n)
{
	struct aio_tcp_session_t *session;
	session = (struct aio_tcp_session_t *)s;

	assert(session->ref > 0);
	session->active = time64_now();
	return aio_socket_send_v(session->socket, vec, n, aio_tcp_transport_onsend, session);
}

int aio_tcp_transport_addref(void* s)
{
	struct aio_tcp_session_t *session;
	session = (struct aio_tcp_session_t *)s;
	return InterlockedIncrement(&session->ref);
}

int aio_tcp_transport_release(void* s)
{
	struct aio_tcp_session_t *session;
	session = (struct aio_tcp_session_t *)s;

	if(0 == InterlockedDecrement(&session->ref))
	{
#if defined(DEBUG) || defined(_DEBUG)
		struct aio_tcp_transport_t *transport;
		transport = session->transport;
		InterlockedDecrement(&transport->ref);
		memset(session, 0xCC, sizeof(*session));
#endif

		free(session);
	}

	return 0;
}
