#include "tls-socket.h"
#include "sockutil.h"
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>
#include <stdlib.h>

struct tls_socket_t
{
    socket_t tcp;
    SSL_CTX *ctx;
    SSL* ssl;
};

int tls_socket_init()
{
    SSL_library_init();
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_ssl_algorithms();
    return 0;
}

int tls_socket_cleanup()
{
    EVP_cleanup();
    return 0;
}

int tls_socket_close(struct tls_socket_t* tls)
{
    if(tls)
    {
        if(tls->ssl)
        {
            SSL_shutdown(tls->ssl);
            SSL_free(tls->ssl);
            tls->ssl = NULL;
        }
        
        if(tls->ctx)
        {
            SSL_CTX_free(tls->ctx);
            tls->ctx = NULL;
        }
        
        if(socket_invalid != tls->tcp && 0 != tls->tcp)
        {
            socket_close(tls->tcp);
            tls->tcp = socket_invalid;
        }
        
        free(tls);
    }
    
    return 0;
}

int tls_socket_getfd(tls_socket_t* tls)
{
    return SSL_get_fd(tls->ssl);
}

tls_socket_t* tls_socket_bind(const char* ip, unsigned int port)
{
    struct tls_socket_t* tls;
    tls = (struct tls_socket_t*)calloc(1, sizeof(*tls));
    
    tls->tcp = socket_tcp_listen(AF_UNSPEC, ip, (unsigned short)port, 64, 0, 1);
    if(socket_invalid == tls->tcp)
    {
        tls_socket_close(tls);
        return NULL;
    }
    
    tls->ctx = SSL_CTX_new(SSLv23_client_method());
    if (!tls->ctx)
    {
        tls_socket_close(tls);
        return NULL;
    }
    
    socket_setnonblock(tls->tcp, 0);
    tls->ssl = SSL_new(tls->ctx);
    SSL_set_fd(tls->ssl, tls->tcp);
    //r = SSL_accept(tls->ssl);
    return tls;
}

static int tls_connect(struct tls_socket_t* tls)
{
	int r;
	tls->ctx = SSL_CTX_new(SSLv23_client_method());
	if (!tls->ctx)
		return -1;

	socket_setnonblock(tls->tcp, 0);
	tls->ssl = SSL_new(tls->ctx);
	SSL_set_fd(tls->ssl, tls->tcp);
	r = SSL_connect(tls->ssl);
	return 1 == r ? 0 : -1;
}

struct tls_socket_t* tls_socket_connect(const char* host, unsigned int port, int timeout)
{
    struct tls_socket_t* tls;
    tls = (struct tls_socket_t*)calloc(1, sizeof(*tls));
    
    tls->tcp = socket_connect_host(host, (u_short)port, timeout);
    if(socket_invalid == tls->tcp)
    {
		tls_socket_close(tls);
        return NULL;
    }
    
	if(0 == tls_connect(tls))
		return tls;
    
	ERR_get_error();
    tls_socket_close(tls);
    return NULL;
}

tls_socket_t* tls_socket_connect2(const struct sockaddr* addr, int timeout)
{
	int r;
	struct tls_socket_t* tls;
	tls = (struct tls_socket_t*)calloc(1, sizeof(*tls));

	tls->tcp = socket_tcp();
	r = socket_connect_by_time(tls->tcp, addr, socket_addr_len(addr), timeout);
	if (0 != r)
	{
		tls_socket_close(tls);
		return NULL;
	}

	if(0 == tls_connect(tls))
		return tls;
    
    ERR_print_errors_fp(stderr);
    tls_socket_close(tls);
    return NULL;
}

tls_socket_t* tls_socket_connect3(int fd)
{
    struct tls_socket_t* tls;
    tls = (struct tls_socket_t*)calloc(1, sizeof(*tls));
    tls->tcp = fd;
    if(0 == tls_connect(tls))
        return tls;
    
    ERR_print_errors_fp(stderr);
    tls_socket_close(tls);
    return NULL;
}

static int tls_accept(struct tls_socket_t* tls)
{
    int r;
    tls->ctx = SSL_CTX_new(SSLv23_client_method());
    if (!tls->ctx)
        return -1;
    
    socket_setnonblock(tls->tcp, 0);
    tls->ssl = SSL_new(tls->ctx);
    SSL_set_fd(tls->ssl, tls->tcp);
    r = SSL_accept(tls->ssl);
    return 1 == r ? 0 : -1;
}

tls_socket_t* tls_socket_accept(int fd)
{
    struct tls_socket_t* tls;
    tls = (struct tls_socket_t*)calloc(1, sizeof(*tls));
    tls->tcp = fd;
    if(0 == tls_accept(tls))
        return tls;
    
    ERR_print_errors_fp(stderr);
    tls_socket_close(tls);
    return NULL;
}

int tls_socket_read(tls_socket_t* tls, void* data, int bytes)
{
    return SSL_read(tls->ssl, data, bytes);
}

int tls_socket_write(tls_socket_t* tls, const void* data, int bytes)
{
    return SSL_write(tls->ssl, data, bytes);
}
