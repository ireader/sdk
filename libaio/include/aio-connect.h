#ifndef _aio_connect_h_
#define _aio_connect_h_

#include "aio-socket.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Connect to host
/// @param[in] host IPv4/IPv6/DNS address
/// @param[in] port tcp port
/// @param[in] timeout connect timeout(MS)
/// @param[in] onconnect user-defined callback, can't be NULL
/// @param[in] param user-defined parameter
/// @return 0-ok, other-error
int aio_connect(const char* host, int port, int timeout, void (*onconnect)(void* param, int code, socket_t tcp, aio_socket_t aio), void* param);

#ifdef __cplusplus
}
#endif
#endif /* !_aio_connect_h_ */
