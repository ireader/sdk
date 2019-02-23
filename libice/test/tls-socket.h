#ifndef tls_socket_h
#define tls_socket_h

#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif
    
typedef struct tls_socket_t tls_socket_t;

int tls_socket_init();
int tls_socket_cleanup();
    
int tls_socket_close(tls_socket_t* tls);
    
/// @param[in] timeout connect timeout(MS)
tls_socket_t* tls_socket_connect(const char* host, unsigned int port, int timeout);

int tls_socket_read(tls_socket_t* tls, void* data, int bytes);
int tls_socket_write(tls_socket_t* tls, const void* data, int bytes);

#if defined(__cplusplus)
}
#endif
#endif /* tls_socket_h */
