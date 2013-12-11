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

typedef void* aio_socket_t;
typedef void (*aio_onaccept)(void* p, int code, const char* ip, int port);
typedef void (*aio_onconnect)(void* p, int code);
typedef void (*aio_ondisconnect)(void* p, int code);
typedef void (*aio_onsend)(void* p, int code, int bytes);
typedef void (*aio_onrecv)(void* p, int code, int bytes);
typedef void (*aio_onrecvfrom)(void* p, int code, int bytes, const char* ip, int port);

int aio_socket_init();
int aio_socket_clean();

/// @param[in] own 1-close socket on aio_socket_close, 0-don't close socket
aio_socket_t aio_socket_create(socket_t socket, int own);
int aio_socket_close(aio_socket_t socket);

/// listen and accept client
/// @param[in] ip local ip address for bind, NULL if bind all
/// @param[in] port local port for bind
/// @param[in] proc callback procedure
/// @param[in] param user-defined parameter
/// @return 0-ok, <0-error
int aio_socket_accept(aio_socket_t socket, aio_onaccept proc, void* param);

/// connect to remote server
/// @param[in] socket aio socket
/// @param[in] ip server ip v4 address
/// @param[in] port server listen port
/// @return 0-ok, <0-error
int aio_socket_connect(aio_socket_t socket, const char* ip, int port, aio_onconnect proc, void* param);

/// aio send
/// @param[in] socket aio socket
/// @param[in] buffer outbound buffer
/// @return 0-ok, <0-error
int aio_socket_send(aio_socket_t socket, const void* buffer, int bytes, aio_onsend proc, void* param);

/// @return 0-ok, <0-error
int aio_socket_recv(aio_socket_t socket, void* buffer, int bytes, aio_onrecv proc, void* param);

/// @return 0-ok, <0-error
int aio_socket_send_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onsend proc, void* param);

/// @return 0-ok, <0-error
int aio_socket_recv_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecv proc, void* param);

/// aio send
/// @param[in] socket aio socket
/// @param[in] buffer outbound buffer
/// @return 0-ok, <0-error
int aio_socket_sendto(aio_socket_t socket, const char* ip, int port, const void* buffer, int bytes, aio_onsend proc, void* param);

/// @return 0-ok, <0-error
int aio_socket_recvfrom(aio_socket_t socket, void* buffer, int bytes, aio_onrecvfrom proc, void* param);

/// @return 0-ok, <0-error
int aio_socket_sendto_v(aio_socket_t socket, const char* ip, int port, socket_bufvec_t* vec, int n, aio_onsend proc, void* param);

/// @return 0-ok, <0-error
int aio_socket_recvfrom_v(aio_socket_t socket, socket_bufvec_t* vec, int n, aio_onrecvfrom proc, void* param);

#endif /* !_aio_socket_h_ */
