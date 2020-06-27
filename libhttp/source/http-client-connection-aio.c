#include "http-client-internal.h"
#include "aio-client.h"
#include <stdlib.h>
#include <errno.h>

#define http_entry(ptr, type, member) ((type*)((char*)ptr-(ptrdiff_t)(&((type*)0)->member)))

struct http_aio_transport_t
{
    struct http_transport_t* transport;
	aio_client_t* client;
	void* buf;
    int len;

	int count;
	socket_bufvec_t vec[2];
    
    struct {
        void (*onsend)(void* param, int code);
        void* param;
    } send;
    
    struct {
        void (*onrecv)(void* param, int code, const void* buf, int len);
        void* param;
    } recv;

	int error;
	int retry;
};

static void http_aio_transport_ondestroy(void* param)
{
    //struct http_aio_transport_t* aio;
    //aio = *(struct http_aio_transport_t**)param;
    (void)param;
}

static void http_aio_transport_onrecv(void* param, int code, size_t bytes)
{
    struct http_aio_transport_t *aio;
    aio = (struct http_aio_transport_t*)param;
    aio->recv.onrecv(aio->recv.param, code, aio->buf, (int)bytes);
}

static void http_aio_transport_onsend(void* param, int code, size_t bytes)
{
    struct http_aio_transport_t *aio;
    aio = (struct http_aio_transport_t*)param;
    aio->send.onsend(aio->send.param, code);
}

static void http_aio_transport_destroy(void* c)
{
    struct http_aio_transport_t* aio;
    aio = (struct http_aio_transport_t*)c;

    if (aio->client)
    {
        aio_client_destroy(aio->client);
        aio->client = NULL;
    }
    free(aio);
}

static void* http_aio_transport_create(struct http_transport_t* transport, const char* scheme, const char* host, int port)
{
	struct http_aio_transport_t* aio;
    aio = http_transport_pool_fetch(transport->priv, scheme, host, port);
    if(aio)
        return aio;
    
	aio = calloc(1, sizeof(struct http_aio_transport_t) + transport->read_buffer_size);
	if (aio)
	{
		struct aio_client_handler_t handler;
		memset(&handler, 0, sizeof(handler));
		handler.ondestroy = http_aio_transport_ondestroy;
		handler.onrecv = http_aio_transport_onrecv;
		handler.onsend = http_aio_transport_onsend;
        
        aio->len = transport->read_buffer_size;
        aio->buf = aio + 1;
        aio->transport = transport;
		aio->client = aio_client_create(host, port, &handler, aio);
	}
	return aio;
}

static int http_aio_transport_close(void* c)
{
    struct http_aio_transport_t* aio;
    aio = (struct http_aio_transport_t*)c;
    return http_transport_pool_put(aio->transport->priv, c, http_aio_transport_destroy);
}

static int http_aio_transport_recv(void* c, void (*onrecv)(void* param, int code, const void* buf, int len), void* param)
{
    struct http_aio_transport_t* aio;
    aio = (struct http_aio_transport_t*)c;
    aio->recv.onrecv = onrecv;
    aio->recv.param = param;
    return aio_client_recv(aio->client, aio->buf, aio->len);
}

static int http_aio_transport_send(void* c, const char* req, int nreq, const void* msg, int bytes, void (*onsend)(void* param, int code), void* param)
{
	struct http_aio_transport_t* aio;
    aio = (struct http_aio_transport_t*)c;
    aio->send.onsend = onsend;
    aio->send.param = param;

	socket_setbufvec(aio->vec, 0, (char*)req, nreq);
	socket_setbufvec(aio->vec, 1, (void*)msg, bytes);
	aio->count = bytes > 0 ? 2 : 1;

	aio->retry = 1; // try again if send failed
	if (0 != aio_client_send_v(aio->client, aio->vec, aio->count))
	{
		// connection reset ???
		aio->retry = 0;
		return aio_client_send_v(aio->client, aio->vec, aio->count);
	}
	return 0;
}

static void http_aio_transport_timeout(void* c, int conn, int recv, int send)
{
    struct http_aio_transport_t* aio;
    aio = (struct http_aio_transport_t*)c;
    aio_client_settimeout(aio->client, conn, recv, send);
}

int http_transport_tcp_aio_init(struct http_transport_t* t)
{
	struct http_transport_pool_t* pool;
    pool = (struct http_transport_pool_t*)calloc(1, sizeof(*pool));
    if(!pool) return -ENOMEM;
    
    memset(t, 0, sizeof(*t));
    t->priv = pool;
    t->is_aio = 1;
    t->keep_alive = 1;
    t->connect_timeout = 5000;
    t->idle_timeout = 2 * 60 * 1000;
    t->idle_connections = 100;
    t->read_buffer_size = 32 * 1024;
    t->write_buffer_size = 16 * 1024;
    
    t->connect = http_aio_transport_create;
    t->close = http_aio_transport_close;
    t->recv = http_aio_transport_recv;
    t->send = http_aio_transport_send;
    t->settimeout = http_aio_transport_timeout;
    return 0;
}
