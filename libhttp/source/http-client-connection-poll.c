#include "http-client-internal.h"
#include "sys/system.h"
#include "sockutil.h"
#if defined(__OPENSSL__)
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#endif
#include <stdlib.h>
#include <errno.h>

struct http_poll_transport_priv_t
{
    struct http_transport_pool_t* pool;
    int (*poll)(void* param, void* c, uintptr_t fd, int event, int timeout, void (*onevent)(void* c, int event));
    void* param;
};

struct http_poll_transport_t
{
    struct http_transport_t* transport;

    socket_t socket;
    struct sockaddr_storage addr;

#if defined(__OPENSSL__)
    SSL_CTX* ctx;
    SSL* ssl;
#endif
    int recv_timeout;
    int send_timeout;
    int conn_timeout;
    int https;
    int connected;

    void* buf;
    int cap;

    struct {
        uint64_t clock;
        socket_bufvec_t vec[2];
        int count;

        void (*onsend)(void* param, int code);
        void* param;
    } send;

    struct {
        uint64_t clock;
        void (*onrecv)(void* param, int code, const void* buf, int len);
        void* param;
    } recv;
};

static void http_poll_transport_onread(void* c, int event);
static void http_poll_transport_onwrite(void* c, int event);

static int http_poll_transport_dosend(struct http_poll_transport_t* tcp)
{
    int i, r, n;
    uint64_t now;
    struct http_poll_transport_priv_t* priv;

#if defined(__OPENSSL__)
    if (tcp->ssl)
    {
        for (r = i = 0; i < tcp->send.count; i++)
        {
            n = SSL_write(tcp->ssl, (void*)tcp->send.vec[i].buf, tcp->send.vec[i].len);
            if (n < 0)
            {
                r = n;
                break;
            }

            r += n;
            if (n != (int)tcp->send.vec[i].len)
                break;
        }
    }
    else
#endif
        r = socket_send_v(tcp->socket, tcp->send.vec, tcp->send.count, 0);

    now = system_clock();
    if (r <= 0)
    {
        tcp->send.onsend(tcp->send.param, r <= 0 ? r : -ETIMEDOUT);
        return 0;
    }

    // remain
    for (i = 0, n = 0; i < tcp->send.count; i++)
    {
        if (n + tcp->send.vec[i].iov_len > (size_t)r)
            break;
        n += tcp->send.vec[i].iov_len;
    }

    if (tcp->send.count > i)
    {
        n = r - n;
        tcp->send.vec[i].iov_len -= n;
        tcp->send.vec[i].iov_base = (char*)tcp->send.vec[i].iov_base + n;
        memmove(&tcp->send.vec[0], &tcp->send.vec[i], sizeof(tcp->send.vec[0]) * (tcp->send.count - i));
        tcp->send.count -= i;
    }
    else
    {
        tcp->send.onsend(tcp->send.param, 0);
        return 0;
    }

    if (now - tcp->send.clock >= tcp->send_timeout)
    {
        tcp->send.onsend(tcp->send.param, -ETIMEDOUT);
        return 0;
    }

    priv = (struct http_poll_transport_priv_t*)tcp->transport->priv;
    return priv->poll(priv->param, tcp, tcp->socket, HTTP_TRANSPORT_POLL_WRITE, tcp->send_timeout - (int)(now - tcp->send.clock), http_poll_transport_onwrite);
}

static int http_poll_transport_dorecv(struct http_poll_transport_t* tcp)
{
    int r;
    uint64_t now;
    struct http_poll_transport_priv_t* priv;

#if defined(__OPENSSL__)
    if (tcp->ssl)
        r = SSL_read(tcp->ssl, tcp->buf, tcp->cap);
    else
#endif
        r = socket_recv(tcp->socket, tcp->buf, tcp->cap, 0);

    now = system_clock();
    if (r >= 0 || now - tcp->recv.clock >= tcp->recv_timeout)
    {
        tcp->recv.onrecv(tcp->recv.param, r >= 0 ? 0 : -ETIMEDOUT, tcp->buf, r);
        return 0;
    }
#if defined(__OPENSSL__)
    else if (tcp->ssl)
    {
        r = SSL_get_error(tcp->ssl, r);
        if(SSL_ERROR_WANT_READ != r)
            return r;
    }
#endif
#if defined(OS_WINDOWS)
    else if (WSAEWOULDBLOCK != WSAGetLastError())
#else
    else if (EWOULDBLOCK != socket_geterror() && EAGAIN != socket_geterror())
#endif
    {
        return r;
    }

    priv = (struct http_poll_transport_priv_t*)tcp->transport->priv;
    return priv->poll(priv->param, tcp, tcp->socket, HTTP_TRANSPORT_POLL_READ, tcp->recv_timeout - (int)(now - tcp->recv.clock), http_poll_transport_onread);
}

static int http_poll_transport_onconnect(void* c, int event)
{
    int r;
    uint64_t now;
    struct http_poll_transport_t* tcp;
    struct http_poll_transport_priv_t* priv;
    tcp = (struct http_poll_transport_t*)c;
    
    now = system_clock();
    if (0 == tcp->connected)
    {
        r = socket_select_connect(tcp->socket, 0);
        if (ETIMEDOUT == r && now - tcp->send.clock < tcp->conn_timeout)
        {
            priv = (struct http_poll_transport_priv_t*)tcp->transport->priv;
            return priv->poll(priv->param, tcp, tcp->socket, HTTP_TRANSPORT_POLL_WRITE, tcp->conn_timeout - (int)(now - tcp->send.clock), http_poll_transport_onconnect);
        }
        else if (0 != r)
        {
            tcp->send.onsend(tcp->send.param, r);
            return 0;
        }

        tcp->connected = 2; // wait for ssl handshake
    }

    if (tcp->https)
    {
#if defined(__OPENSSL__)
        if (!tcp->ctx)
        {
            tcp->ctx = SSL_CTX_new(SSLv23_client_method());
            if (!tcp->ctx)
                return -1;

#if defined(__OPENSSL_VERIFY__)
            // enable peer verify
            SSL_CTX_set_options(tcp->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
            SSL_CTX_set_verify(tcp->ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
            SSL_CTX_set_default_verify_paths(tcp->ctx);
#endif

            tcp->ssl = SSL_new(tcp->ctx);
            SSL_set_fd(tcp->ssl, tcp->socket);
            SSL_set_connect_state(tcp->ssl);
            //r = SSL_connect(tcp->ssl);
            //if (1 != r)
            //    return -1;
        }

        r = SSL_do_handshake(tcp->ssl);
        if (1 == r)
        {
            // done
            tcp->connected = 1;
            return http_poll_transport_dosend(tcp);
        }
        else
        {
            if (now - tcp->send.clock >= tcp->conn_timeout)
            {
                tcp->send.onsend(tcp->send.param, -ETIMEDOUT);
                return 0;
            }

            priv = (struct http_poll_transport_priv_t*)tcp->transport->priv;
            r = SSL_get_error(tcp->ssl, r);
            if (r == SSL_ERROR_WANT_WRITE)
            {
                return priv->poll(priv->param, tcp, tcp->socket, HTTP_TRANSPORT_POLL_WRITE, tcp->conn_timeout - (int)(now - tcp->send.clock), http_poll_transport_onconnect);
            }
            else if (r == SSL_ERROR_WANT_READ)
            {
                return priv->poll(priv->param, tcp, tcp->socket, HTTP_TRANSPORT_POLL_READ, tcp->conn_timeout - (int)(now - tcp->send.clock), http_poll_transport_onconnect);
            }
            else
            {
                tcp->send.onsend(tcp->send.param, r);
            }
        }
#else
        assert(0);
#endif
    }
    else
    {
        tcp->connected = 1;
        return http_poll_transport_dosend(tcp);
    }

    (void)event;
    return 0;
}

static void http_poll_transport_onread(void* c, int event)
{
    int r;
    struct http_poll_transport_t* tcp;
    tcp = (struct http_poll_transport_t*)c;
    assert(HTTP_TRANSPORT_POLL_READ == event);
    r = http_poll_transport_dorecv(tcp);
    if (0 != r)
        tcp->recv.onrecv(tcp->recv.param, r, NULL, 0);
}

static void http_poll_transport_onwrite(void* c, int event)
{
    int r;
    struct http_poll_transport_t* tcp;
    tcp = (struct http_poll_transport_t*)c;
    assert(HTTP_TRANSPORT_POLL_WRITE == event);
    r = http_poll_transport_dosend(tcp);
    if (0 != r)
    {
        assert(r <= 0);
        tcp->send.onsend(tcp->send.param, r);
    }
}

static int http_poll_transport_connect(struct http_poll_transport_t* tcp)
{
    int r;
    struct http_poll_transport_priv_t* priv;

    assert(0 == tcp->connected);
    tcp->send.clock = system_clock();

    // check connection
    if (socket_invalid != tcp->socket && 1 == socket_readable(tcp->socket))
    {
        socket_close(tcp->socket);
        tcp->socket = socket_invalid;
    }

    if (socket_invalid == tcp->socket)
    {
        tcp->socket = socket(tcp->addr.ss_family, SOCK_STREAM, 0);
        socket_setnonblock(tcp->socket, 1);
        r = socket_connect(tcp->socket, (struct sockaddr*)&tcp->addr, socket_addr_len((struct sockaddr*)&tcp->addr));
        if (0 != r)
        {
#if defined(OS_WINDOWS)
            if (WSAEWOULDBLOCK == WSAGetLastError())
#else
            if (EINPROGRESS == errno)
#endif
            {
                priv = (struct http_poll_transport_priv_t*)tcp->transport->priv;
                return priv->poll(priv->param, tcp, tcp->socket, HTTP_TRANSPORT_POLL_WRITE, tcp->conn_timeout, http_poll_transport_onconnect);
            }
            else
            {
                return r;
            }
        }
    }

    return http_poll_transport_onconnect(tcp, HTTP_TRANSPORT_POLL_WRITE);
}

static void http_poll_transport_destroy(void* c)
{
    struct http_poll_transport_t* tcp;
    tcp = (struct http_poll_transport_t*)c;


#if defined(__OPENSSL__)
    if (tcp->ssl)
    {
        SSL_shutdown(tcp->ssl);
        SSL_free(tcp->ssl);
        tcp->ssl = NULL;
    }

    if (tcp->ctx)
    {
        SSL_CTX_free(tcp->ctx);
        tcp->ctx = NULL;
    }
#endif

    if (socket_invalid != tcp->socket)
    {
        socket_close(tcp->socket);
        tcp->socket = socket_invalid;
    }

    free(c);
}

static void* http_poll_transport_create(struct http_transport_t* transport, const char* scheme, const char* host, int port)
{
    struct sockaddr_storage addr;
    struct http_poll_transport_t* tcp;
    if (0 != socket_addr_from(&addr, NULL, host, (unsigned short)port))
        return NULL;

    tcp = http_transport_pool_fetch(transport->priv, scheme, host, port);
    if (tcp)
        return tcp;

    tcp = calloc(1, sizeof(struct http_poll_transport_t) + transport->read_buffer_size);
    if (!tcp) return NULL;

    memcpy(&tcp->addr, &addr, sizeof(tcp->addr));
    tcp->socket = socket_invalid;
    tcp->recv_timeout = 20000;
    tcp->send_timeout = 20000;
    tcp->conn_timeout = transport->connect_timeout;
    tcp->transport = transport;
    tcp->cap = transport->read_buffer_size;
    tcp->buf = tcp + 1;
    tcp->https = 0 == strcmp("https", scheme) ? 1 : 0;
    return tcp;
}

static int http_poll_transport_close(void* c)
{
    struct http_poll_transport_t* tcp;
    tcp = (struct http_poll_transport_t*)c;
    return http_transport_pool_put(tcp->transport->priv, c, http_poll_transport_destroy);
}

static int http_poll_transport_recv(void* c, void (*onrecv)(void* param, int code, const void* buf, int len), void* param)
{
    struct http_poll_transport_t* tcp;
    tcp = (struct http_poll_transport_t*)c;
    tcp->recv.clock = system_clock();
    tcp->recv.onrecv = onrecv;
    tcp->recv.param = param;
    if (0 == tcp->connected)
    {
        assert(0); // should call http_poll_transport_send first
        return -1;
    }
    return http_poll_transport_dorecv(tcp);
}

static int http_poll_transport_send(void* c, const char* req, int nreq, const void* msg, int bytes, void (*onsend)(void* param, int code), void* param)
{
    struct http_poll_transport_t* tcp;
    tcp = (struct http_poll_transport_t*)c;
    socket_setbufvec(tcp->send.vec, 0, (char*)req, nreq);
    socket_setbufvec(tcp->send.vec, 1, (void*)msg, bytes);
    tcp->send.count = bytes > 0 ? 2 : 1;
    tcp->send.onsend = onsend;
    tcp->send.param = param;
    tcp->send.clock = system_clock();
    return 1 != tcp->connected ? http_poll_transport_connect(tcp) : http_poll_transport_dosend(tcp);
}

static uintptr_t http_poll_transport_getfd(void* c)
{
    struct http_poll_transport_t* tcp;
    tcp = (struct http_poll_transport_t*)c;
    return (uintptr_t)tcp->socket;
}

static void http_poll_transport_timeout(void* c, int conn, int recv, int send)
{
    struct http_poll_transport_t* tcp;
    tcp = (struct http_poll_transport_t*)c;
    assert(conn >= 0 && recv >= 0 && send >= 0);
    tcp->conn_timeout = conn;
    tcp->recv_timeout = recv;
    tcp->send_timeout = send;
}

struct http_transport_t* http_transport_user_poll(int (*poll)(void* param, void* c, uintptr_t fd, int event, int timeout, void (*onevent)(void* c, int event)), void* param)
{
    struct http_transport_t* t;
    struct http_transport_pool_t* pool;
    struct http_poll_transport_priv_t* priv;
    pool = (struct http_transport_pool_t*)calloc(1, sizeof(*pool));
    if (!pool) return NULL;

    t = (struct http_transport_t*)calloc(1, sizeof(*t) + sizeof(struct http_poll_transport_priv_t));
    if (!t) return NULL;
    t->priv = (void*)(t + 1);
    t->is_aio = 1;
    t->keep_alive = 1;
    t->connect_timeout = 5000;
    t->idle_timeout = 2 * 60 * 1000;
    t->idle_connections = 100;
    t->read_buffer_size = 32 * 1024;
    t->write_buffer_size = 16 * 1024;

    t->connect = http_poll_transport_create;
    t->close = http_poll_transport_close;
    t->recv = http_poll_transport_recv;
    t->send = http_poll_transport_send;
    t->settimeout = http_poll_transport_timeout;

    priv = (struct http_poll_transport_priv_t*)(t->priv);
    priv->pool = pool;
    priv->poll = poll;
    priv->param = param;
    return t;
}
