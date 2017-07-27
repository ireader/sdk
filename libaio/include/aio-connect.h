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
void aio_connect(const char* host, int port, int timeout, void (*onconnect)(void* param, aio_socket_t aio, int code), void* param);

#ifdef __cplusplus
}
#endif
#endif /* !_aio_connect_h_ */
