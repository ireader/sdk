#include "sockutil.h"
#include "sys/onetime.h"
#include "http-transport.h"
#include "http-client-internal.h"
#include <stdlib.h>

struct http_tcp_transport_t
{
    struct http_transport_t* transport;
    
    socket_t socket;
    int recv_timeout;
    int send_timeout;
    int conn_timeout;
    
    void* buf;
    int len;
};

static void http_tcp_transport_destroy(void* c)
{
    struct http_tcp_transport_t* tcp;
    tcp = (struct http_tcp_transport_t*)c;
    
    if (socket_invalid != tcp->socket)
    {
        socket_close(tcp->socket);
        tcp->socket = socket_invalid;
    }
    
    free(c);
}

static int http_tcp_transport_connect(struct http_tcp_transport_t* tcp, const char* scheme, const char* host, int port)
{
    // check connection
    if(socket_invalid != tcp->socket && 1==socket_readable(tcp->socket))
    {
        socket_close(tcp->socket);
        tcp->socket = socket_invalid;
    }

    if(socket_invalid == tcp->socket)
    {
        socket_t socket;
        socket = socket_connect_host(host, (u_short)port, tcp->conn_timeout);
        if(socket_invalid == socket)
            return -1;

        socket_setnonblock(socket, 0); // restore block status
        tcp->socket = socket;
    }

    return 0;
}

static void* http_tcp_transport_create(struct http_transport_t* transport, const char* scheme, const char* host, int port)
{
    struct http_tcp_transport_t* tcp;
    tcp = http_transport_pool_fetch(transport->priv, scheme, host, port);
    if(tcp)
        return tcp;
        
    tcp = calloc(1, sizeof(struct http_tcp_transport_t) + transport->read_buffer_size);
    if(!tcp) return NULL;
    
    tcp->socket = socket_invalid;
    tcp->recv_timeout = 20000;
	tcp->send_timeout = 20000;
    tcp->conn_timeout = transport->connect_timeout;
    tcp->transport = transport;
    tcp->len = transport->read_buffer_size;
    tcp->buf = tcp + 1;
    
    if(0 != http_tcp_transport_connect(tcp, scheme, host, port))
    {
        http_tcp_transport_destroy(tcp);
        return NULL;
    }
    
	return tcp;
}

static int http_tcp_transport_close(void* c)
{
    struct http_tcp_transport_t* tcp;
    tcp = (struct http_tcp_transport_t*)c;
    return http_transport_pool_put(tcp->transport->priv, c, http_tcp_transport_destroy);
}

static int http_tcp_transport_send(void* c, const char* req, int nreq, const void* msg, int bytes, void (*onsend)(void* param, int code), void* param)
{
    int r;
	socket_bufvec_t vec[2];
    struct http_tcp_transport_t* tcp;
    tcp = (struct http_tcp_transport_t*)c;

	socket_setbufvec(vec, 0, (void*)req, nreq);
	socket_setbufvec(vec, 1, (void*)msg, bytes);
    r = socket_send_v_all_by_time(tcp->socket, vec, bytes > 0 ? 2 : 1, 0, tcp->send_timeout);
    onsend(param, r == (int)(nreq + bytes) ? 0 : -1);
    return 0;
}

static int http_tcp_transport_recv(void* c, void (*onrecv)(void* param, int code, const void* buf, int len), void* param)
{
    int r;
    struct http_tcp_transport_t* tcp;
    tcp = (struct http_tcp_transport_t*)c;
    
    r = socket_recv_by_time(tcp->socket, tcp->buf, tcp->len, 0, tcp->recv_timeout);
    onrecv(param, r >= 0 ? 0 : r, tcp->buf, r);
    return 0;
}

static void http_tcp_transport_timeout(void* c, int conn, int recv, int send)
{
	struct http_tcp_transport_t* tcp;
    tcp = (struct http_tcp_transport_t*)c;
    assert(conn >= 0 && recv >= 0 && send >= 0);
	tcp->conn_timeout = conn;
	tcp->recv_timeout = recv;
	tcp->send_timeout = send;
}

int http_transport_tcp(struct http_transport_t* t)
{
    struct http_transport_pool_t* pool;
    pool = (struct http_transport_pool_t*)calloc(1, sizeof(*pool));
    if(!pool) return -ENOMEM;
    
    memset(t, 0, sizeof(*t));
    t->priv = pool;
    t->is_aio = 0;
    t->keep_alive = 1;
    t->connect_timeout = 5000;
    t->idle_timeout = 2 * 60 * 1000;
    t->idle_connections = 100;
    t->read_buffer_size = 32 * 1024;
    t->write_buffer_size = 16 * 1024;
    
    t->connect = http_tcp_transport_create;
    t->close = http_tcp_transport_close;
    t->recv = http_tcp_transport_recv;
    t->send = http_tcp_transport_send;
    t->settimeout = http_tcp_transport_timeout;
    return 0;
}

static onetime_t s_once;
static struct http_transport_t s_default;
static struct http_transport_t s_default_aio;
void http_transport_default_init(void)
{
    http_transport_tcp(&s_default);
    http_transport_tcp_aio(&s_default_aio);
}

struct http_transport_t* http_transport_default(void)
{
    onetime_exec(&s_once, http_transport_default_init);
    return &s_default;
}

struct http_transport_t* http_transport_default_aio(void)
{
    onetime_exec(&s_once, http_transport_default_init);
    return &s_default_aio;
}
