#include "cstringext.h"
#include "aio-socket.h"
#include "sys/atomic.h"
#include "sys/spinlock.h"
#include "aio-udp-transport.h"
#include "list.h"

#if defined(_DEBUG) || defined(DEBUG)
#define CHECK_MODE
#endif

//#define MAX_UDP_PACKET 65536 // 64KB
#define MAX_UDP_PACKET 8192 // 8KB
//#define MAX_UDP_PACKET 576 // UNIX Network Programming by W. Richard Stevens

struct aio_udp_session_t
{
	volatile int32_t ref;

	char msg[MAX_UDP_PACKET];

	struct aio_udp_transport_t *transport;

	char ip[32]; // IPv4/IPv6
	int port;

	void *data;

#if defined(CHECK_MODE)
	struct list_head node;
#endif
};

struct aio_udp_transport_t
{
	spinlock_t locker;
	volatile int closed;
	volatile int used;

	volatile int32_t ref;
	volatile aio_socket_t socket;

	struct aio_udp_transport_handler_t handler;
	void* ptr;

#if defined(CHECK_MODE)
	int32_t num;
	struct list_head head;
#endif
};

static int aio_udp_transport_start(struct aio_udp_transport_t *transport);

static int aio_udp_transport_unlink(struct aio_udp_transport_t *transport)
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

static struct aio_udp_session_t* aio_udp_session_create(struct aio_udp_transport_t *transport)
{
	struct aio_udp_session_t *session;

	session = (struct aio_udp_session_t*)malloc(sizeof(*session));
	if(session)
	{
		memset(session, 0, sizeof(*session));
		session->ref = 1;
		session->transport = transport;

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

static int aio_udp_session_destroy(struct aio_udp_session_t *session)
{
	struct aio_udp_transport_t *transport;
	transport = session->transport;

#if defined(CHECK_MODE)
	spinlock_lock(&transport->locker);
	list_remove(&session->node);
	spinlock_unlock(&transport->locker);
	atomic_decrement32(&transport->num);
#endif

	aio_udp_transport_unlink(transport);

	assert(0 == session->ref);
#if defined(DEBUG) || defined(_DEBUG)
	memset(session, 0xCC, sizeof(*session));
#endif
	free(session);
	return 0;
}

static void aio_udp_transport_onrecv(void* param, int code, size_t bytes, const char* ip, int port)
{
	struct aio_udp_session_t *session;
	struct aio_udp_transport_t *transport;
	session = (struct aio_udp_session_t *)param;
	transport = session->transport;
	assert(session->ref > 0 && transport->ref > 0);

	if(0 != code)
	{
		assert(0);
	}
	else
	{
		assert(0 != bytes);
		session->port = port;
		strncpy(session->ip, ip, sizeof(session->ip));
		transport->handler.onrecv(transport->ptr, session, session->msg, bytes, ip, port, &session->data);

        aio_udp_transport_start(transport);
	}

	aio_udp_transport_release(session);
}

static void aio_udp_transport_onsend(void* param, int code, size_t bytes)
{
	struct aio_udp_session_t *session;
	struct aio_udp_transport_t *transport;
	session = (struct aio_udp_session_t *)param;
	transport = session->transport;
	transport->handler.onsend(transport->ptr, session, session->data, code, bytes);
    aio_udp_transport_release(session);
}

static int aio_udp_transport_start(struct aio_udp_transport_t *transport)
{
	int r = 0;
    int closed = 0;
	struct aio_udp_session_t *session;
    assert(transport->ref > 0);

    // check status
    spinlock_lock(&transport->locker);
    closed = transport->closed;
    if(0 == closed) ++transport->used;
    spinlock_unlock(&transport->locker);

    if(0 == closed)
    {
        session = aio_udp_session_create(transport);
        if(!session)
        {
            assert(0);
            return -1;
        }
        else
        {
            r = aio_socket_recvfrom(transport->socket, session->msg, sizeof(session->msg), aio_udp_transport_onrecv, session);
            
            spinlock_lock(&transport->locker);
            --transport->used;
            assert(transport->used >= 0);
            spinlock_unlock(&transport->locker);

            if(0 != r)
            {
                assert(0);
                aio_udp_transport_release(session);
                return -1;
            }
        }
    }

	return 0;
}

void* aio_udp_transport_create(socket_t socket, const struct aio_udp_transport_handler_t *handler, void* ptr)
{
	int r;
	struct aio_udp_transport_t *transport;
	transport = (struct aio_udp_transport_t*)malloc(sizeof(*transport));
	if(!transport)
		return NULL;

	memset(transport, 0, sizeof(*transport));
	spinlock_create(&transport->locker);
	transport->ref = 1;
#if defined(CHECK_MODE)
	transport->num = 0;
	LIST_INIT_HEAD(&transport->head);
#endif
    memcpy(&transport->handler, handler, sizeof(transport->handler));
    transport->ptr = ptr;

    transport->socket = aio_socket_create(socket, 1);
	if(invalid_aio_socket == transport->socket)
	{
		aio_udp_transport_destroy(transport);
		return NULL;
	}

	// start recv
	r = aio_udp_transport_start(transport);
	if(0 != r)
	{
		aio_udp_transport_destroy(transport);
		return NULL;
	}

	return transport;
}

int aio_udp_transport_destroy(void* t)
{
	struct aio_udp_transport_t *transport;
	transport = (struct aio_udp_transport_t *)t;

	spinlock_lock(&transport->locker);
	assert(transport->socket);
	transport->closed = 1; // mark closed by user
	if(0 == transport->used)
	{
		aio_socket_destroy(transport->socket);
		transport->socket = NULL;
	}
	spinlock_unlock(&transport->locker);

	return aio_udp_transport_unlink(transport);
}

int aio_udp_transport_send(void* s, const void* msg, size_t bytes)
{
	struct aio_udp_session_t *session;
	struct aio_udp_transport_t *transport;
	session = (struct aio_udp_session_t *)s;
	transport = session->transport;

	assert(session->ref > 0 && 0 == transport->closed && transport->socket);
    atomic_increment32(&session->ref);
    return aio_socket_sendto(transport->socket, session->ip, session->port, msg, bytes, aio_udp_transport_onsend, session);
}

int aio_udp_transport_sendv(void* s, socket_bufvec_t *vec, int n)
{
	struct aio_udp_session_t *session;
	struct aio_udp_transport_t *transport;
	session = (struct aio_udp_session_t *)s;
	transport = session->transport;

	assert(session->ref > 0 && 0 == transport->closed && transport->socket);
    atomic_increment32(&session->ref);
	return aio_socket_sendto_v(transport->socket, session->ip, session->port, vec, n, aio_udp_transport_onsend, session);
}

int aio_udp_transport_addref(void* s)
{
	struct aio_udp_session_t *session;
	session = (struct aio_udp_session_t *)s;
    return atomic_increment32(&session->ref);
}

int aio_udp_transport_release(void* s)
{
	struct aio_udp_session_t *session;
	session = (struct aio_udp_session_t *)s;

	if(0 == atomic_decrement32(&session->ref))
	{
		aio_udp_session_destroy(session);
	}

	return 0;
}
