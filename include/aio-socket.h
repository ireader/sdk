#ifndef _aio_socket_h_
#define _aio_socket_h_

#if defined(OS_LINUX)
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>

typedef int socket_t;
typedef struct iovec socket_bufvec_t;
#else
#include <WinSock2.h>

typedef SOCKET socket_t;
typedef WSABUF socket_bufvec_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* aio_socket_t;

/// aio_socket_accept callback
/// @param[in] param user-defined parameter
/// @param[in] code 0-ok, other-error, ip/port value undefined
/// @param[in] ip remote client ip address
/// @param[in] port remote client ip port
typedef void (*aio_onaccept)(void* param, int code, socket_t socket, const char* ip, int port);

/// aio_socket_connect callback
/// @param[in] param user-defined parameter
/// @param[in] code 0-ok, other-error
typedef void (*aio_onconnect)(void* param, int code);

/// aio_socket_send/aio_socket_send_v/aio_socket_sendto/aio_socket_sendto_v callback
/// @param[in] param user-defined parameter
/// @param[in] code 0-ok, other-error
/// @param[in] bytes 0-means socket closed, >0-send bytes
typedef void (*aio_onsend)(void* param, int code, int bytes); 

/// aio_socket_recv/aio_socket_recv_v callback
/// @param[in] param user-defined parameter
/// @param[in] code 0-ok, other-error
/// @param[in] bytes 0-means socket closed, >0-received bytes
typedef void (*aio_onrecv)(void* param, int code, int bytes);

/// aio_socket_recvfrom/aio_socket_recvfrom_v callback
/// @param[in] param user-defined parameter
/// @param[in] code 0-ok, other-error
/// @param[in] bytes 0-means socket closed, >0-transfered bytes
/// @param[in] ip remote client ip address
/// @param[in] port remote client ip port
typedef void (*aio_onrecvfrom)(void* param, int code, int bytes, const char* ip, int port);

/// aio initialization
/// @param[in] threads max concurrent thread call aio_socket_process
/// @param[in] timeout aio process timeout
/// @return 0-ok, other-error
int aio_socket_init(int threads, int timeout);

/// aio cleanup
/// @return 0-ok, other-error
int aio_socket_clean();

/// aio worker
/// @return 0-timeout, <0-error, >0-work number
int aio_socket_process();

/// @param[in] own 1-close socket on aio_socket_close, 0-don't close socket
aio_socket_t aio_socket_create(socket_t socket, int own);

/// close aio-socket
/// Remark: don't call any callback after this function
/// @return 0-ok, other-error
int aio_socket_destroy(aio_socket_t socket);

/// listen and accept client
/// @param[in] ip local ip address for bind, NULL if bind all
/// @param[in] port local port for bind
/// @param[in] proc callback procedure
/// @param[in] param user-defined parameter
/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_accept(aio_socket_t socket, aio_onaccept proc, void* param);

/// connect to remote server
/// @param[in] socket aio socket
/// @param[in] ip server ip v4 address
/// @param[in] port server listen port
/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_connect(aio_socket_t socket, const char* ip, int port, aio_onconnect proc, void* param);

/// aio send
/// @param[in] socket aio socket
/// @param[in] buffer outbound buffer
/// @param[in] bytes buffer size
/// @param[in] proc user-defined callback
/// @param[in] param user-defined parameter
/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_send(aio_socket_t socket, const void* buffer, int bytes, aio_onsend proc, void* param);

/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_recv(aio_socket_t socket, void* buffer, int bytes, aio_onrecv proc, void* param);

/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_send_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onsend proc, void* param);

/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_recv_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecv proc, void* param);

/// aio send
/// @param[in] socket aio socket
/// @param[in] ip remote host ip address
/// @param[in] port remote host port
/// @param[in] buffer outbound buffer
/// @param[in] bytes buffer size
/// @param[in] proc user-defined callback
/// @param[in] param user-defined parameter
/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_sendto(aio_socket_t socket, const char* ip, int port, const void* buffer, int bytes, aio_onsend proc, void* param);

/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_recvfrom(aio_socket_t socket, void* buffer, int bytes, aio_onrecvfrom proc, void* param);

/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_sendto_v(aio_socket_t socket, const char* ip, int port, socket_bufvec_t* vec, int n, aio_onsend proc, void* param);

/// @return 0-ok, <0-error, don't call proc if return error
int aio_socket_recvfrom_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecvfrom proc, void* param);

#ifdef __cplusplus
}
#endif

#endif /* !_aio_socket_h_ */
