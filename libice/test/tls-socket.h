#ifndef tls_socket_h
#define tls_socket_h

#include <stdio.h>
#include "sys/sock.h"

#if defined(__cplusplus)
extern "C" {
#endif
    
typedef struct tls_socket_t tls_socket_t;

int tls_socket_init();
int tls_socket_cleanup();
    
int tls_socket_close(tls_socket_t* tls);
    
tls_socket_t* tls_socket_accept(int fd);
    
/// @param[in] timeout connect timeout(MS)
tls_socket_t* tls_socket_connect(const char* host, unsigned int port, int timeout);
tls_socket_t* tls_socket_connect2(const struct sockaddr* addr, int timeout);
tls_socket_t* tls_socket_connect3(int fd);

int tls_socket_read(tls_socket_t* tls, void* data, int bytes);
int tls_socket_write(tls_socket_t* tls, const void* data, int bytes);

int tls_socket_getfd(tls_socket_t* tls);
    
#if defined(__cplusplus)
}
#endif
#endif /* tls_socket_h */
