#ifndef _aio_socket_h_
#define _aio_socket_h_

#ifndef OS_SOCKET_TYPE
#if defined(OS_WINDOWS)
#include <WinSock2.h>
#include <WS2tcpip.h> // socklen_t

typedef SOCKET socket_t;
typedef WSABUF socket_bufvec_t;
#else
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>

typedef int socket_t;
typedef struct iovec socket_bufvec_t;
#endif

#define OS_SOCKET_TYPE
#endif /* OS_SOCKET_TYPE */

#ifdef __cplusplus
extern "C" {
#endif

#define invalid_aio_socket NULL

typedef void* aio_socket_t;

typedef void (*aio_ondestroy)(void* param);

/// aio_socket_accept callback
/// @param[in] param user-defined parameter
/// @param[in] code 0-ok, other-error, ip/port value undefined
/// @param[in] addr peer socket address(IPv4/IPv6)
/// @param[in] addrlen peer socket address length in bytes
typedef void (*aio_onaccept)(void* param, int code, socket_t socket, const struct sockaddr* addr, socklen_t addrlen);

/// aio_socket_connect callback
/// @param[in] param user-defined parameter
/// @param[in] code 0-ok, other-error
typedef void (*aio_onconnect)(void* param, int code);

/// aio_socket_send/aio_socket_send_v/aio_socket_sendto/aio_socket_sendto_v callback
/// @param[in] param user-defined parameter
/// @param[in] code 0-ok, other-error
/// @param[in] bytes 0-means socket closed, >0-send bytes
typedef void (*aio_onsend)(void* param, int code, size_t bytes); 

/// aio_socket_recv/aio_socket_recv_v callback
/// @param[in] param user-defined parameter
/// @param[in] code 0-ok, other-error
/// @param[in] bytes 0-means socket closed, >0-received bytes
typedef void (*aio_onrecv)(void* param, int code, size_t bytes);

/// aio_socket_recvfrom/aio_socket_recvfrom_v callback
/// @param[in] param user-defined parameter
/// @param[in] code 0-ok, other-error
/// @param[in] bytes 0-means socket closed, >0-transfered bytes
/// @param[in] addr peer socket address(IPv4/IPv6)
/// @param[in] addrlen peer socket address length in bytes
typedef void (*aio_onrecvfrom)(void* param, int code, size_t bytes, const struct sockaddr* addr, socklen_t addrlen);

/// aio initialization
/// @param[in] threads max concurrent thread call aio_socket_process
/// @return 0-ok, other-error
int aio_socket_init(int threads);

/// aio cleanup
/// @return 0-ok, other-error
int aio_socket_clean(void);

/// aio worker
/// @param[in] timeout aio process timeout
/// @return 0-timeout, <0-error, >0-work number
int aio_socket_process(int timeout);

/// @param[in] own 1-close socket on aio_socket_close, 0-don't close socket
/// @return NULL-error, other-ok
aio_socket_t aio_socket_create(socket_t socket, int own);

/// close aio-socket
/// Remark: don't call any callback after this function
/// @return 0-ok, other-error
int aio_socket_destroy(aio_socket_t socket, aio_ondestroy ondestroy, void* param);

/// listen and accept client
/// @param[in] proc callback procedure
/// @param[in] param user-defined parameter
/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_accept(aio_socket_t socket, aio_onaccept proc, void* param);

/// connect to remote server
/// @param[in] socket aio socket
/// @param[in] addr peer socket address(IPv4 or IPv6)
/// @param[in] addrlen addr length in bytes
/// @return 0-ok, 10022-don't bound(see remark), <0-error don't call proc if return error
/// Remark: windows socket need bind() port before connect. e.g. socket_bind_any(socket, 0);
int aio_socket_connect(aio_socket_t socket, const struct sockaddr *addr, socklen_t addrlen, aio_onconnect proc, void* param);

/// aio send
/// @param[in] socket aio socket
/// @param[in] buffer outbound buffer
/// @param[in] bytes buffer size
/// @param[in] proc user-defined callback
/// @param[in] param user-defined parameter
/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_send(aio_socket_t socket, const void* buffer, size_t bytes, aio_onsend proc, void* param);

/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_recv(aio_socket_t socket, void* buffer, size_t bytes, aio_onrecv proc, void* param);

/// @return 0-ok, <0-error, don't call proc if return error
/// @param[in] vec buffer array(must valid before aio_onsend callback)
/// @param[in] n vec item number
int aio_socket_send_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onsend proc, void* param);

/// @return 0-ok, <0-error, don't call proc if return error
/// @param[in] vec buffer array(must valid before aio_onsend callback)
/// @param[in] n vec item number
int aio_socket_recv_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecv proc, void* param);

/// aio udp send
/// @param[in] socket aio socket
/// @param[in] addr peer socket address(IPv4 or IPv6)
/// @param[in] addrlen addr length in bytes
/// @param[in] buffer outbound buffer
/// @param[in] bytes buffer size
/// @param[in] proc user-defined callback
/// @param[in] param user-defined parameter
/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_sendto(aio_socket_t socket, const struct sockaddr *addr, socklen_t addrlen, const void* buffer, size_t bytes, aio_onsend proc, void* param);

/// aio udp recv
/// @param[out] buffer data buffer(must valid before aio_onrecvfrom callback)
/// @param[out] bytes buffer size
/// @param[in] proc user-defined callback
/// @param[in] param user-defined parameter
/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_recvfrom(aio_socket_t socket, void* buffer, size_t bytes, aio_onrecvfrom proc, void* param);

/// aio udp send
/// @param[in] socket aio socket
/// @param[in] addr peer socket address(IPv4 or IPv6)
/// @param[in] addrlen addr length in bytes
/// @param[in] vec buffer array(must valid before aio_onsend callback)
/// @param[in] n vec item number
/// @param[in] proc user-defined callback
/// @param[in] param user-defined parameter
/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_sendto_v(aio_socket_t socket, const struct sockaddr *addr, socklen_t addrlen, socket_bufvec_t* vec, int n, aio_onsend proc, void* param);

/// aio udp recv
/// @param[in] socket aio socket
/// @param[in] vec buffer array(must valid before aio_onrecvfrom callback)
/// @param[in] n vec item number
/// @param[in] proc user-defined callback
/// @param[in] param user-defined parameter
/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_recvfrom_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecvfrom proc, void* param);

#ifdef __cplusplus
}
#endif

#endif /* !_aio_socket_h_ */
