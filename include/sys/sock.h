#ifndef _platform_socket_h_
#define _platform_socket_h_

#if defined(OS_WINDOWS)
#include <Winsock2.h>
#include <WS2tcpip.h>
#include <ws2ipdef.h>

#ifndef OS_SOCKET_TYPE
typedef SOCKET	socket_t;
typedef WSABUF	socket_bufvec_t;
#define OS_SOCKET_TYPE
#endif /* OS_SOCKET_TYPE */

#define socket_invalid	INVALID_SOCKET
#define socket_error	SOCKET_ERROR

#if defined(_MSC_VER)
#pragma comment(lib, "Ws2_32.lib")
#pragma warning(push)
#pragma warning(disable: 6031) // warning C6031: Return value ignored: 'snprintf'
#endif

#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH

// IPv6 MTU
#ifndef IPV6_MTU_DISCOVER
	#define IPV6_MTU_DISCOVER	71
	#define IP_PMTUDISC_DO		1
	#define IP_PMTUDISC_DONT	2
#endif

#else
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>

#ifndef OS_SOCKET_TYPE
typedef int socket_t;
typedef struct iovec socket_bufvec_t;
#define OS_SOCKET_TYPE
#endif /* OS_SOCKET_TYPE */

#define socket_invalid	-1
#define socket_error	-1

#endif

#include <assert.h>
#include <stdio.h>

#ifndef IN
#define IN 
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

#define SOCKET_ADDRLEN INET6_ADDRSTRLEN

static inline int socket_init(void);
static inline int socket_cleanup(void);
static inline int socket_geterror(void);

static inline socket_t socket_tcp(void);
static inline socket_t socket_udp(void);
static inline socket_t socket_raw(void);
static inline socket_t socket_rdm(void);
static inline socket_t socket_tcp_ipv6(void);
static inline socket_t socket_udp_ipv6(void);
static inline socket_t socket_raw_ipv6(void);
// @return 0-ok, <0-socket_error(by socket_geterror())
static inline int socket_shutdown(socket_t sock, int flag); // SHUT_RD/SHUT_WR/SHUT_RDWR
static inline int socket_close(socket_t sock);

// @return 0-ok, <0-socket_error(by socket_geterror())
static inline int socket_connect(IN socket_t sock, IN const struct sockaddr* addr, IN socklen_t addrlen);
// MSDN: When using bind with the SO_EXCLUSIVEADDR or SO_REUSEADDR socket option, the socket option must be set prior to executing bind to have any affect
static inline int socket_bind(IN socket_t sock, IN const struct sockaddr* addr, IN socklen_t addrlen);
static inline int socket_listen(IN socket_t sock, IN int backlog);
// @return >=0-accepted socket, <0-socket_invalid(by socket_geterror())
static inline socket_t socket_accept(IN socket_t sock, OUT struct sockaddr_storage* ss, OUT socklen_t* addrlen);

// socket read/write
// @return >0-sent/received bytes, <0-socket_error(by socket_geterror()), 0-peer shutdown(recv only)
static inline int socket_send(IN socket_t sock, IN const void* buf, IN size_t len, IN int flags);
static inline int socket_recv(IN socket_t sock, OUT void* buf, IN size_t len, IN int flags);
static inline int socket_sendto(IN socket_t sock, IN const void* buf, IN size_t len, IN int flags, IN const struct sockaddr* to, IN socklen_t tolen);
static inline int socket_recvfrom(IN socket_t sock, OUT void* buf, IN size_t len, IN int flags, OUT struct sockaddr* from, OUT socklen_t* fromlen);

// @return >0-sent/received bytes, <0-socket_error(by socket_geterror()), 0-peer shutdown(recv only)
static inline int socket_send_v(IN socket_t sock, IN const socket_bufvec_t* vec, IN int n, IN int flags);
static inline int socket_recv_v(IN socket_t sock, IN socket_bufvec_t* vec, IN int n, IN int flags);
static inline int socket_sendto_v(IN socket_t sock, IN const socket_bufvec_t* vec, IN int n, IN int flags, IN const struct sockaddr* to, IN socklen_t tolen);
static inline int socket_recvfrom_v(IN socket_t sock, IN socket_bufvec_t* vec, IN int n, IN int flags, IN struct sockaddr* from, IN socklen_t* fromlen);

// Linux: 1. select may update the timeout argument to indicate how much time was left
//        2. This interval will be rounded up to the system clock granularity, and kernel scheduling delays mean that the blocking interval may overrun by a small amount.
// @return 0-timeout, >0-available fds, <0-socket_error(by socket_geterror())
static inline int socket_select(IN int n, IN fd_set* rfds, IN fd_set* wfds, IN fd_set* efds, IN struct timeval* timeout); // timeout: NULL-forever, 0-immediately
static inline int socket_select_readfds(IN int n, IN fd_set* fds, IN struct timeval* timeout); // timeout: NULL-forever, 0-immediately
static inline int socket_select_writefds(IN int n, IN fd_set* fds, IN struct timeval* timeout); // timeout: NULL-forever, 0-immediately
static inline int socket_select_read(IN socket_t sock, IN int timeout); // timeout: >0-milliseconds, 0-immediately, <0-forever
static inline int socket_select_write(IN socket_t sock, IN int timeout); // timeout: >0-milliseconds, 0-immediately, <0-forever
static inline int socket_readable(IN socket_t sock);
static inline int socket_writeable(IN socket_t sock);

/// @return 0-ok, other-error
static inline int socket_select_connect(IN socket_t sock, IN int timeout); // timeout: >0-milliseconds, 0-immediately, <0-forever

// socket options
// @return 0-ok, <0-socket_error(by socket_geterror())
static inline int socket_setkeepalive(IN socket_t sock, IN int enable); // keep alive
static inline int socket_getkeepalive(IN socket_t sock, OUT int* enable);
static inline int socket_setlinger(IN socket_t sock, IN int onoff, IN int seconds); // linger
static inline int socket_getlinger(IN socket_t sock, OUT int* onoff, OUT int* seconds);
static inline int socket_setsendbuf(IN socket_t sock, IN size_t size); // send buf
static inline int socket_getsendbuf(IN socket_t sock, OUT size_t* size);
static inline int socket_setrecvbuf(IN socket_t sock, IN size_t size); // recv buf
static inline int socket_getrecvbuf(IN socket_t sock, OUT size_t* size);
static inline int socket_setsendtimeout(IN socket_t sock, IN size_t seconds); // send timeout
static inline int socket_getsendtimeout(IN socket_t sock, OUT size_t* seconds);
static inline int socket_setrecvtimeout(IN socket_t sock, IN size_t seconds); // recv timeout
static inline int socket_getrecvtimeout(IN socket_t sock, OUT size_t* seconds);
static inline int socket_setreuseaddr(IN socket_t sock, IN int enable); // reuse addr. 
static inline int socket_getreuseaddr(IN socket_t sock, OUT int* enable);
static inline int socket_setreuseport(IN socket_t sock, IN int enable); // reuse port. 
static inline int socket_getreuseport(IN socket_t sock, OUT int* enable);
static inline int socket_setipv6only(IN socket_t sock, IN int ipv6_only); // 1-ipv6 only, 0-both ipv4 and ipv6
static inline int socket_getdomain(IN socket_t sock, OUT int* domain); // get socket protocol address family(sock don't need bind)
static inline int socket_setpriority(IN socket_t sock, IN int priority);
static inline int socket_getpriority(IN socket_t sock, OUT int* priority);
static inline int socket_settos(IN socket_t sock, IN int dscp); // ipv4 only
static inline int socket_gettos(IN socket_t sock, OUT int* dscp); // ipv4 only
static inline int socket_settclass(IN socket_t sock, IN int dscp); // ipv6 only
static inline int socket_gettclass(IN socket_t sock, OUT int* dscp); // ipv6 only
static inline int socket_setttl(IN socket_t sock, IN int ttl); // ipv4 only
static inline int socket_getttl(IN socket_t sock, OUT int* ttl); // ipv4 only
static inline int socket_setttl6(IN socket_t sock, IN int ttl); // ipv6 only
static inline int socket_getttl6(IN socket_t sock, OUT int* ttl); // ipv6 only
static inline int socket_setdontfrag(IN socket_t sock, IN int dontfrag); // ipv4 udp only
static inline int socket_getdontfrag(IN socket_t sock, OUT int* dontfrag); // ipv4 udp only
static inline int socket_setdontfrag6(IN socket_t sock, IN int dontfrag); // ipv6 udp only
static inline int socket_getdontfrag6(IN socket_t sock, OUT int* dontfrag); // ipv6 udp only
static inline int socket_setpktinfo(IN socket_t sock, IN int enable); // ipv4 udp only
static inline int socket_setpktinfo6(IN socket_t sock, IN int enable); // ipv6 udp only

// socket status
// @return 0-ok, <0-socket_error(by socket_geterror())
static inline int socket_setcork(IN socket_t sock, IN int cork); // 1-cork, 0-uncork
static inline int socket_setnonblock(IN socket_t sock, IN int noblock); // non-block io, 0-block, 1-nonblock
static inline int socket_setnondelay(IN socket_t sock, IN int nodelay); // non-delay io(Nagle Algorithm), 0-delay, 1-nodelay
static inline int socket_getunread(IN socket_t sock, OUT size_t* size); // MSDN: Use to determine the amount of data pending in the network's input buffer that can be read from socket s

static inline int socket_getname(IN socket_t sock, OUT char ip[SOCKET_ADDRLEN], OUT u_short* port); // must be bound/connected
static inline int socket_getpeername(IN socket_t sock, OUT char ip[SOCKET_ADDRLEN], OUT u_short* port);

// socket utility
static inline int socket_isip(IN const char* ip); // socket_isip("192.168.1.2") -> 1, socket_isip("www.google.com") -> 0
static inline int socket_ipv4(IN const char* ipv4_or_dns, OUT char ip[SOCKET_ADDRLEN]);
static inline int socket_ipv6(IN const char* ipv6_or_dns, OUT char ip[SOCKET_ADDRLEN]);

static inline int socket_addr_from_ipv4(OUT struct sockaddr_in* addr4, IN const char* ip_or_dns, IN u_short port);
static inline int socket_addr_from_ipv6(OUT struct sockaddr_in6* addr6, IN const char* ip_or_dns, IN u_short port);
static inline int socket_addr_from(OUT struct sockaddr_storage* ss, OUT socklen_t* len, IN const char* ipv4_or_ipv6_or_dns, IN u_short port);
static inline int socket_addr_to(IN const struct sockaddr* sa, IN socklen_t salen, OUT char ip[SOCKET_ADDRLEN], OUT u_short* port);
static inline int socket_addr_name(IN const struct sockaddr* sa, IN socklen_t salen, OUT char* host, IN socklen_t hostlen);
static inline int socket_addr_setport(IN struct sockaddr* sa, IN socklen_t salen, u_short port);
static inline int socket_addr_is_multicast(IN const struct sockaddr* sa, IN socklen_t salen);
static inline int socket_addr_compare(const struct sockaddr* first, const struct sockaddr* second); // 0-equal, other-don't equal
static inline int socket_addr_len(const struct sockaddr* addr);

static inline void socket_setbufvec(INOUT socket_bufvec_t* vec, IN int idx, IN void* ptr, IN size_t len);
static inline void socket_getbufvec(IN const socket_bufvec_t* vec, IN int idx, OUT void** ptr, OUT size_t* len);

// multicast
static inline int socket_multicast_join(IN socket_t sock, IN const char* group, IN const char* local);
static inline int socket_multicast_leave(IN socket_t sock, IN const char* group, IN const char* local);
static inline int socket_multicast_join_source(IN socket_t sock, IN const char* group, IN const char* source, IN const char* local);
static inline int socket_multicast_leave_source(IN socket_t sock, IN const char* group, IN const char* source, IN const char* local);
static inline int socket_multicast_join6(IN socket_t sock, IN const char* group);
static inline int socket_multicast_leave6(IN socket_t sock, IN const char* group);

//////////////////////////////////////////////////////////////////////////
///
/// socket create/close 
/// 
//////////////////////////////////////////////////////////////////////////
static inline int socket_init(void)
{
#if defined(OS_WINDOWS)
	WORD wVersionRequested;
	WSADATA wsaData;
	
	wVersionRequested = MAKEWORD(2, 2);
	return WSAStartup(wVersionRequested, &wsaData);
#else
	return 0;
#endif
}

static inline int socket_cleanup(void)
{
#if defined(OS_WINDOWS)
	return WSACleanup();
#else
	return 0;
#endif
}

static inline int socket_geterror(void)
{
#if defined(OS_WINDOWS)
	return WSAGetLastError();
#else
	return errno;
#endif
}

static inline socket_t socket_tcp(void)
{
	return socket(PF_INET, SOCK_STREAM, 0);
}

static inline socket_t socket_udp(void)
{
	return socket(PF_INET, SOCK_DGRAM, 0);
}

static inline socket_t socket_raw(void)
{
	return socket(PF_INET, SOCK_RAW, IPPROTO_RAW);
}

static inline socket_t socket_rdm(void)
{
	return socket(PF_INET, SOCK_RDM, 0);
}

static inline socket_t socket_tcp_ipv6(void)
{
	return socket(PF_INET6, SOCK_STREAM, 0);
}

static inline socket_t socket_udp_ipv6(void)
{
	return socket(PF_INET6, SOCK_DGRAM, 0);
}

static inline socket_t socket_raw_ipv6(void)
{
	return socket(PF_INET6, SOCK_RAW, IPPROTO_RAW);
}

static inline int socket_shutdown(socket_t sock, int flag)
{
	return shutdown(sock, flag);
}

static inline int socket_close(socket_t sock)
{
#if defined(OS_WINDOWS)
	// MSDN:
	// If closesocket fails with WSAEWOULDBLOCK the socket handle is still valid, 
	// and a disconnect is not initiated. The application must call closesocket again to close the socket. 
	return closesocket(sock);
#else
	return close(sock);
#endif
}


//////////////////////////////////////////////////////////////////////////
///
/// socket operation
/// 
//////////////////////////////////////////////////////////////////////////
static inline int socket_connect(IN socket_t sock, IN const struct sockaddr* addr, IN socklen_t addrlen)
{
	return connect(sock, addr, addrlen);
}

static inline int socket_bind(IN socket_t sock, IN const struct sockaddr* addr, IN socklen_t addrlen)
{
	return bind(sock, addr, addrlen);
}

static inline int socket_listen(IN socket_t sock, IN int backlog)
{
	return listen(sock, backlog);
}

static inline socket_t socket_accept(IN socket_t sock, OUT struct sockaddr_storage* addr, OUT socklen_t* addrlen)
{
	*addrlen = sizeof(struct sockaddr_storage);
	return accept(sock, (struct sockaddr*)addr, addrlen);
}

//////////////////////////////////////////////////////////////////////////
///
/// socket read/write
/// 
//////////////////////////////////////////////////////////////////////////
static inline int socket_send(IN socket_t sock, IN const void* buf, IN size_t len, IN int flags)
{
#if defined(OS_WINDOWS)
	return send(sock, (const char*)buf, (int)len, flags);
#else
	return (int)send(sock, buf, len, flags);
#endif
}

static inline int socket_recv(IN socket_t sock, OUT void* buf, IN size_t len, IN int flags)
{
#if defined(OS_WINDOWS)
	return recv(sock, (char*)buf, (int)len, flags);
#else
	return (int)recv(sock, buf, len, flags);
#endif
}

static inline int socket_sendto(IN socket_t sock, IN const void* buf, IN size_t len, IN int flags, IN const struct sockaddr* to, IN socklen_t tolen)
{
#if defined(OS_WINDOWS)
	return sendto(sock, (const char*)buf, (int)len, flags, to, tolen);
#else
	return (int)sendto(sock, buf, len, flags, to, tolen);
#endif
}

static inline int socket_recvfrom(IN socket_t sock, OUT void* buf, IN size_t len, IN int flags, OUT struct sockaddr* from, OUT socklen_t* fromlen)
{
#if defined(OS_WINDOWS)
	return recvfrom(sock, (char*)buf, (int)len, flags, from, fromlen);
#else
	return (int)recvfrom(sock, buf, len, flags, from, fromlen);
#endif
}

static inline int socket_send_v(IN socket_t sock, IN const socket_bufvec_t* vec, IN int n, IN int flags)
{
#if defined(OS_WINDOWS)
	DWORD count = 0;
	int r = WSASend(sock, (socket_bufvec_t*)vec, (DWORD)n, &count, flags, NULL, NULL);
	return 0 == r ? (int)count : r;
#else
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = (struct iovec*)vec;
	msg.msg_iovlen = n;
	return (int)sendmsg(sock, &msg, flags);
#endif
}

static inline int socket_recv_v(IN socket_t sock, IN socket_bufvec_t* vec, IN int n, IN int flags)
{
#if defined(OS_WINDOWS)
	DWORD count = 0;
	int r = WSARecv(sock, vec, (DWORD)n, &count, (LPDWORD)&flags, NULL, NULL);
	return 0 == r ? (int)count : r;
#else
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = vec;
	msg.msg_iovlen = n;
	return (int)recvmsg(sock, &msg, flags);
#endif
}

static inline int socket_sendto_v(IN socket_t sock, IN const socket_bufvec_t* vec, IN int n, IN int flags, IN const struct sockaddr* to, IN socklen_t tolen)
{
#if defined(OS_WINDOWS)
	DWORD count = 0;
	int r = WSASendTo(sock, (socket_bufvec_t*)vec, (DWORD)n, &count, flags, to, tolen, NULL, NULL);
	return 0 == r ? (int)count : r;
#else
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (struct sockaddr*)to;
	msg.msg_namelen = tolen;
	msg.msg_iov = (struct iovec*)vec;
	msg.msg_iovlen = n;
	return (int)sendmsg(sock, &msg, flags);
#endif
}

static inline int socket_recvfrom_v(IN socket_t sock, IN socket_bufvec_t* vec, IN int n, IN int flags, IN struct sockaddr* from, IN socklen_t* fromlen)
{
#if defined(OS_WINDOWS)
	DWORD count = 0;
	return 0 == WSARecvFrom(sock, vec, (DWORD)n, &count, (LPDWORD)&flags, from, fromlen, NULL, NULL) ? (int)count : SOCKET_ERROR;
#else
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = from;
	msg.msg_namelen = *fromlen;
	msg.msg_iov = vec;
	msg.msg_iovlen = n;
	return (int)recvmsg(sock, &msg, flags);
#endif
}

static inline int socket_select(IN int n, IN fd_set* rfds, IN fd_set* wfds, IN fd_set* efds, IN struct timeval* timeout)
{
#if defined(OS_WINDOWS)
	return select(n, rfds, wfds, efds, timeout);
#else
	int r = select(n, rfds, wfds, efds, timeout);
	while(-1 == r && (EINTR == errno || EAGAIN == errno))
		r = select(n, rfds, wfds, efds, timeout);
	return r;
#endif
}

static inline int socket_select_readfds(IN int n, IN fd_set* fds, IN struct timeval* timeout)
{
	return socket_select(n, fds, NULL, NULL, timeout);
}

static inline int socket_select_writefds(IN int n, IN fd_set* fds, IN struct timeval* timeout)
{
	return socket_select(n, NULL, fds, NULL, timeout);
}

static inline int socket_select_read(IN socket_t sock, IN int timeout)
{
#if defined(OS_WINDOWS)
	fd_set fds;
	struct timeval tv;
	assert(socket_invalid != sock); // linux: FD_SET error
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	tv.tv_sec = timeout/1000;
	tv.tv_usec = (timeout%1000) * 1000;
	return socket_select_readfds(0 /*sock+1*/, &fds, timeout<0?NULL:&tv);
#else
	int r;
	struct pollfd fds;

	fds.fd = sock;
	fds.events = POLLIN;
	fds.revents = 0;

	r = poll(&fds, 1, timeout);
	while(-1 == r && (EINTR == errno || EAGAIN == errno))
		r = poll(&fds, 1, timeout);
	return r;
#endif
}

static inline int socket_select_write(IN socket_t sock, IN int timeout)
{
#if defined(OS_WINDOWS)
	fd_set fds;
	struct timeval tv;

	assert(socket_invalid != sock); // linux: FD_SET error

	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	tv.tv_sec = timeout/1000;
	tv.tv_usec = (timeout%1000) * 1000;
	return socket_select_writefds(0 /*sock+1*/, &fds, timeout<0?NULL:&tv);
#else
	int r;
	struct pollfd fds;

	fds.fd = sock;
	fds.events = POLLOUT;
	fds.revents = 0;

	r = poll(&fds, 1, timeout);
	while(-1 == r && (EINTR == errno || EAGAIN == errno))
		r = poll(&fds, 1, timeout);
	return r;
#endif
}

static inline int socket_select_connect(IN socket_t sock, IN int timeout)
{
	int r;
	int errcode;
	int errlen = sizeof(errcode);

#if defined(OS_WINDOWS)
	fd_set wfds, efds;
	struct timeval tv;

	assert(socket_invalid != sock); // linux: FD_SET error

	FD_ZERO(&wfds);
	FD_ZERO(&efds);
	FD_SET(sock, &wfds);
	FD_SET(sock, &efds);

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	// MSDN > connect function > Remarks:
	//	If the client uses the select function, success is reported in the writefds set 
	//	and failure is reported in the exceptfds set.
	// MSDN > select function > Remarks:
	//	writefds: If processing a connect call (nonblocking), connection has succeeded.
	//	exceptfds: If processing a connect call (nonblocking), connection attempt failed.
	r = socket_select(0 /*sock+1*/, NULL, &wfds, &efds, timeout < 0 ? NULL : &tv);
	if (1 == r)
	{
		if (FD_ISSET(sock, &wfds))
			return 0;
		r = getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&errcode, &errlen);
		return 0 == r ? errcode : WSAGetLastError();
	}
	return 0 == r ? ETIMEDOUT /*WSAETIMEDOUT*/ : WSAGetLastError();
#else
	// https://linux.die.net/man/2/connect
	// The socket is nonblocking and the connection cannot be
	// completed immediately.  It is possible to select(2) or poll(2)
	// for completion by selecting the socket for writing.  After
	// select(2) indicates writability, use getsockopt(2) to read the
	// SO_ERROR option at level SOL_SOCKET to determine whether
	// connect() completed successfully (SO_ERROR is zero) or
	// unsuccessfully (SO_ERROR is one of the usual error codes
	// listed here, explaining the reason for the failure).
	r = socket_select_write(sock, timeout);
	if (1 == r)
	{
		r = getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)&errcode, (socklen_t*)&errlen);
		return 0 == r ? errcode : errno;
	}
	return 0 == r ? ETIMEDOUT : errno;
#endif
}

static inline int socket_readable(IN socket_t sock)
{
	return socket_select_read(sock, 0);
}

static inline int socket_writeable(IN socket_t sock)
{
	return socket_select_write(sock, 0);
}

//////////////////////////////////////////////////////////////////////////
///
/// socket options
/// 
//////////////////////////////////////////////////////////////////////////
static inline int socket_setopt_bool(IN socket_t sock, IN int optname, IN int enable)
{
#if defined(OS_WINDOWS)
	BOOL v = enable ? TRUE : FALSE;
	return setsockopt(sock, SOL_SOCKET, optname, (const char*)&v, sizeof(v));
#else
	return setsockopt(sock, SOL_SOCKET, optname, &enable, sizeof(enable));
#endif
}

static inline int socket_getopt_bool(IN socket_t sock, IN int optname, OUT int* enable)
{
	socklen_t len;
#if defined(OS_WINDOWS)
	int r;
	BOOL v;
	len = sizeof(v);
	r = getsockopt(sock, SOL_SOCKET, optname, (char*)&v, &len);
	if(0 == r)
		*enable = (TRUE==v)?1:0;
	return r;
#else
	len = sizeof(*enable);
	return getsockopt(sock, SOL_SOCKET, optname, enable, &len);
#endif
}

static inline int socket_setkeepalive(IN socket_t sock, IN int enable)
{
	return socket_setopt_bool(sock, SO_KEEPALIVE, enable);
}

static inline int socket_getkeepalive(IN socket_t sock, OUT int* enable)
{
	return socket_getopt_bool(sock, SO_KEEPALIVE, enable);
}

static inline int socket_setlinger(IN socket_t sock, IN int onoff, IN int seconds)
{
	struct linger l;
	l.l_onoff = (u_short)onoff;
	l.l_linger = (u_short)seconds;
	return setsockopt(sock, SOL_SOCKET, SO_LINGER, (const char*)&l, sizeof(l));
}

static inline int socket_getlinger(IN socket_t sock, OUT int* onoff, OUT int* seconds)
{
	int r;
	socklen_t len;
	struct linger l;
	
	len = sizeof(l);
	r = getsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&l, &len);
	if(0 == r)
	{
		*onoff = l.l_onoff;
		*seconds = l.l_linger;
	}
	return r;
}

static inline int socket_setsendbuf(IN socket_t sock, IN size_t size)
{
	return setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&size, sizeof(size));
}

static inline int socket_getsendbuf(IN socket_t sock, OUT size_t* size)
{
	socklen_t len;
	len = sizeof(*size);
	return getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)size, &len);
}

static inline int socket_setrecvbuf(IN socket_t sock, IN size_t size)
{
	return setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&size, sizeof(size));
}

static inline int socket_getrecvbuf(IN socket_t sock, OUT size_t* size)
{
	socklen_t len;
	len = sizeof(*size);
	return getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)size, &len);
}

static inline int socket_setsendtimeout(IN socket_t sock, IN size_t seconds)
{
#if defined(OS_WINDOWS)
	return setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&seconds, sizeof(seconds));
#else
	struct timeval tv;
	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	return setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

static inline int socket_getsendtimeout(IN socket_t sock, OUT size_t* seconds)
{
	socklen_t len;
#if defined(OS_WINDOWS)
	len = sizeof(*seconds);
	return getsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)seconds, &len);
#else
	int r;
	struct timeval tv;
	len = sizeof(tv);
	r = getsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, &len);
	if(0 == r)
		*seconds = tv.tv_sec;
	return r;
#endif
}

static inline int socket_setrecvtimeout(IN socket_t sock, IN size_t seconds)
{
#if defined(OS_WINDOWS)
	return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&seconds, sizeof(seconds));
#else
	struct timeval tv;
	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

static inline int socket_getrecvtimeout(IN socket_t sock, OUT size_t* seconds)
{
	socklen_t len;
#if defined(OS_WINDOWS)
	len = sizeof(*seconds);
	return getsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)seconds, &len);
#else
	int r;
	struct timeval tv;
	len = sizeof(tv);
	r = getsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, &len);
	if(0 == r)
		*seconds = tv.tv_sec;
	return r;
#endif
}

static inline int socket_setreuseaddr(IN socket_t sock, IN int enable)
{
	// https://stackoverflow.com/questions/14388706/socket-options-so-reuseaddr-and-so-reuseport-how-do-they-differ-do-they-mean-t
	// https://www.cnblogs.com/xybaby/p/7341579.html
	// Windows: SO_REUSEADDR = SO_REUSEADDR + SO_REUSEPORT
	return socket_setopt_bool(sock, SO_REUSEADDR, enable);
}

static inline int socket_getreuseaddr(IN socket_t sock, OUT int* enable)
{
	return socket_getopt_bool(sock, SO_REUSEADDR, enable);
}

#if defined(SO_REUSEPORT)
static inline int socket_setreuseport(IN socket_t sock, IN int enable)
{
	return socket_setopt_bool(sock, SO_REUSEPORT, enable);
}

static inline int socket_getreuseport(IN socket_t sock, OUT int* enable)
{
	return socket_getopt_bool(sock, SO_REUSEPORT, enable);
}
#endif

#if defined(TCP_CORK)
// 1-cork, 0-uncork
static inline int socket_setcork(IN socket_t sock, IN int cork)
{
    //return setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, &cork, sizeof(cork));
    return setsockopt(sock, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));
}
#endif

static inline int socket_setnonblock(IN socket_t sock, IN int noblock)
{
	// 0-block, 1-no-block
#if defined(OS_WINDOWS)
	return ioctlsocket(sock, FIONBIO, (u_long*)&noblock);
#else
	// http://stackoverflow.com/questions/1150635/unix-nonblocking-i-o-o-nonblock-vs-fionbio
	// Prior to standardization there was ioctl(...FIONBIO...) and fcntl(...O_NDELAY...) ...
	// POSIX addressed this with the introduction of O_NONBLOCK.
	int flags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, noblock ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
	//return ioctl(sock, FIONBIO, &noblock);
#endif
}

static inline int socket_setnondelay(IN socket_t sock, IN int nodelay)
{
	// 0-delay(enable the Nagle algorithm)
	// 1-no-delay(disable the Nagle algorithm)
	// http://linux.die.net/man/7/tcp
	return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
}

static inline int socket_getunread(IN socket_t sock, OUT size_t* size)
{
#if defined(OS_WINDOWS)
	return ioctlsocket(sock, FIONREAD, (u_long*)size);
#else
	return ioctl(sock, FIONREAD, (int*)size);
#endif
}

static inline int socket_setipv6only(IN socket_t sock, IN int ipv6_only)
{
	// Windows Vista or later: default 1
	// https://msdn.microsoft.com/en-us/library/windows/desktop/ms738574%28v=vs.85%29.aspx
	// Linux 2.4.21 and 2.6: /proc/sys/net/ipv6/bindv6only defalut 0
	// http://www.man7.org/linux/man-pages/man7/ipv6.7.html
	return setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&ipv6_only, sizeof(ipv6_only));
}

#if defined(OS_LINUX)
static inline int socket_setpriority(IN socket_t sock, IN int priority)
{
	return setsockopt(sock, SOL_SOCKET, SO_PRIORITY, (const char*)&priority, sizeof(priority));
}

static inline int socket_getpriority(IN socket_t sock, OUT int* priority)
{
	socklen_t len;
	len = sizeof(int);
	return getsockopt(sock, SOL_SOCKET, SO_PRIORITY, (char*)priority, &len);
}

// ipv4 only
static inline int socket_settos(IN socket_t sock, IN int dscp)
{
	// Winsock IP_TOS option is no longer available, The call will always be ignored silently.
	// https://blogs.msdn.microsoft.com/wndp/2006/07/05/deprecating-old-qos-apis/

	//https://en.wikipedia.org/wiki/Type_of_service
	//http://www.bogpeople.com/networking/dscp.shtml
	dscp <<= 2; // 0 - ECN (Explicit Congestion Notification)
	return setsockopt(sock, IPPROTO_IP, IP_TOS, (const char*)&dscp, sizeof(dscp));
}

// ipv4 only
static inline int socket_gettos(IN socket_t sock, OUT int* dscp)
{
	int r;
	socklen_t len;
	len = sizeof(int);
	r = getsockopt(sock, IPPROTO_IP, IP_TOS, (char*)&dscp, &len);
	if (0 == r)
		*dscp >>= 2; // skip ECN (Explicit Congestion Notification)
	return r;
}

// ipv6 only
static inline int socket_settclass(IN socket_t sock, IN int dscp)
{
	dscp <<= 2; // 0 - ECN (Explicit Congestion Notification)
	return setsockopt(sock, IPPROTO_IPV6, IPV6_TCLASS, (const char*)&dscp, sizeof(dscp));
}

// ipv6 only
static inline int socket_gettclass(IN socket_t sock, OUT int* dscp)
{
	int r;
	socklen_t len;
	len = sizeof(int);
	r = getsockopt(sock, IPPROTO_IPV6, IPV6_TCLASS, (char*)&dscp, &len);
	if (0 == r)
		*dscp >>= 2; // skip ECN (Explicit Congestion Notification)
	return r;
}
#endif

// ipv4 udp only
static inline int socket_setdontfrag(IN socket_t sock, IN int dontfrag)
{
#if defined(OS_WINDOWS)
	DWORD v = (DWORD)dontfrag;
	return setsockopt(sock, IPPROTO_IP, IP_DONTFRAGMENT, (const char*)&v, sizeof(DWORD));
#elif defined(OS_LINUX)
	dontfrag = dontfrag ? IP_PMTUDISC_DO : IP_PMTUDISC_WANT;
	return setsockopt(sock, IPPROTO_IP, IP_MTU_DISCOVER, &dontfrag, sizeof(dontfrag));
#else
	return -1;
#endif
}

// ipv4 udp only
static inline int socket_getdontfrag(IN socket_t sock, OUT int* dontfrag)
{
	int r = -1;
#if defined(OS_WINDOWS)
	DWORD v;
	int n = sizeof(DWORD);
	r = getsockopt(sock, IPPROTO_IP, IP_DONTFRAGMENT, (char*)&v, &n);
	*dontfrag = (int)v;
#elif defined(OS_LINUX)
	socklen_t n = sizeof(int);
	r = getsockopt(sock, IPPROTO_IP, IP_MTU_DISCOVER, dontfrag, &n);
	*dontfrag = IP_PMTUDISC_DO == *dontfrag ? 1 : 0;
#endif
	return r;
}

// ipv6 udp only
static inline int socket_setdontfrag6(IN socket_t sock, IN int dontfrag)
{
#if defined(OS_WINDOWS)
	DWORD v = (DWORD)(dontfrag ? IP_PMTUDISC_DO : IP_PMTUDISC_DONT);
	return setsockopt(sock, IPPROTO_IPV6, IPV6_MTU_DISCOVER, (const char*)&v, sizeof(DWORD));
#elif defined(OS_LINUX)
	dontfrag = dontfrag ? IP_PMTUDISC_DO : IP_PMTUDISC_WANT;
	return setsockopt(sock, IPPROTO_IPV6, IPV6_MTU_DISCOVER, &dontfrag, sizeof(dontfrag));
#else
	return -1;
#endif
}

// ipv6 udp only
static inline int socket_getdontfrag6(IN socket_t sock, OUT int* dontfrag)
{
	int r = -1;
#if defined(OS_WINDOWS)
	DWORD v;
	int n = sizeof(DWORD);
	r = getsockopt(sock, IPPROTO_IPV6, IPV6_MTU_DISCOVER, (char*)&v, &n);
	*dontfrag = IP_PMTUDISC_DO == v ? 1 : 0;;
#elif defined(OS_LINUX)
	socklen_t n = sizeof(int);
	r = getsockopt(sock, IPPROTO_IPV6, IPV6_MTU_DISCOVER, dontfrag, &n);
	*dontfrag = IP_PMTUDISC_DO == *dontfrag ? 1 : 0;
#endif
	return r;
}

// ipv4 udp only
static inline int socket_setpktinfo(IN socket_t sock, IN int enable)
{
#if defined(OS_WINDOWS)
	BOOL v = enable ? TRUE : FALSE;
	return setsockopt(sock, IPPROTO_IP, IP_PKTINFO, (const char*)&v, sizeof(v));
#elif defined(OS_LINUX)
	return setsockopt(sock, IPPROTO_IP, IP_PKTINFO, &enable, sizeof(enable));
#else
	return -1;
#endif
}

// ipv6 udp only
static inline int socket_setpktinfo6(IN socket_t sock, IN int enable)
{
#if defined(OS_WINDOWS)
	BOOL v = enable ? TRUE : FALSE;
	return setsockopt(sock, IPPROTO_IPV6, IPV6_PKTINFO, (const char*)&v, sizeof(v));
#elif defined(OS_LINUX)
	return setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &enable, sizeof(enable));
#else
	return -1;
#endif
}

static inline int socket_setttl(IN socket_t sock, IN int ttl)
{
	return setsockopt(sock, IPPROTO_IP, IP_TTL, (const char*)&ttl, sizeof(ttl));
}

static inline int socket_getttl(IN socket_t sock, OUT int* ttl)
{
	socklen_t len;
	len = sizeof(*ttl);
	return getsockopt(sock, IPPROTO_IP, IP_TTL, (char*)ttl, &len);
}

static inline int socket_setttl6(IN socket_t sock, IN int ttl)
{
	return setsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, (const char*)&ttl, sizeof(ttl));
}

static inline int socket_getttl6(IN socket_t sock, OUT int* ttl)
{
	socklen_t len;
	len = sizeof(*ttl);
	return getsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, (char*)ttl, &len);
}

static inline int socket_getdomain(IN socket_t sock, OUT int* domain)
{
	int r;
#if defined(OS_WINDOWS)
	WSAPROTOCOL_INFOW protocolInfo;
	int len = sizeof(protocolInfo);
	r = getsockopt(sock, SOL_SOCKET, SO_PROTOCOL_INFOW, (char*)&protocolInfo, &len);
	if (0 == r)
		*domain = protocolInfo.iAddressFamily;
#elif defined(OS_LINUX) 
	socklen_t len = sizeof(domain);
	r = getsockopt(sock, SOL_SOCKET, SO_DOMAIN, (char*)domain, &len);
#else
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    r = getsockname(sock, (struct sockaddr*)&addr, &addrlen);
    *domain = addr.ss_family;
#endif
	return r;
}

// must be bound/connected
static inline int socket_getname(IN socket_t sock, OUT char ip[SOCKET_ADDRLEN], OUT u_short* port)
{
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	if(socket_error == getsockname(sock, (struct sockaddr*)&addr, &addrlen))
		return socket_error;

	return socket_addr_to((struct sockaddr*)&addr, addrlen, ip, port);
}

static inline int socket_getpeername(IN socket_t sock, OUT char ip[SOCKET_ADDRLEN], OUT u_short* port)
{
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	if(socket_error == getpeername(sock, (struct sockaddr*)&addr, &addrlen))
		return socket_error;
	
	return socket_addr_to((struct sockaddr*)&addr, addrlen, ip, port);
}

/// @return 1-ok, 0-error
static inline int socket_isip(IN const char* ip)
{
#if 1
	struct sockaddr_storage addr;
	if(1 != inet_pton(AF_INET, ip, &((struct sockaddr_in*)&addr)->sin_addr) 
		&& 1 != inet_pton(AF_INET6, ip, &((struct sockaddr_in6*)&addr)->sin6_addr))
		return 0;
	return 1;
#else
	struct addrinfo hints, *addr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST /*| AI_V4MAPPED | AI_ADDRCONFIG*/;
	if (0 != getaddrinfo(ip, NULL, &hints, &addr))
		return 0;
	freeaddrinfo(&addr);
    return 1;
#endif
}

static inline int socket_ipv4(IN const char* ipv4_or_dns, OUT char ip[SOCKET_ADDRLEN])
{
	int r;
	struct addrinfo hints, *addr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
//	hints.ai_flags = AI_ADDRCONFIG;
	r = getaddrinfo(ipv4_or_dns, NULL, &hints, &addr);
	if (0 != r)
		return r;

	assert(AF_INET == addr->ai_family);
	inet_ntop(AF_INET, &(((struct sockaddr_in*)addr->ai_addr)->sin_addr), ip, SOCKET_ADDRLEN);
	freeaddrinfo(addr);
	return 0;
}

static inline int socket_ipv6(IN const char* ipv6_or_dns, OUT char ip[SOCKET_ADDRLEN])
{
	int r;
	struct addrinfo hints, *addr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_flags = /*AI_ADDRCONFIG |*/ AI_V4MAPPED; // map ipv4 address to ipv6
	r = getaddrinfo(ipv6_or_dns, NULL, &hints, &addr);
	if (0 != r)
		return r;

	assert(AF_INET6 == addr->ai_family);
	inet_ntop(AF_INET6, &(((struct sockaddr_in6*)addr->ai_addr)->sin6_addr), ip, SOCKET_ADDRLEN);
	freeaddrinfo(addr);
	return 0;
}

static inline int socket_addr_from_ipv4(OUT struct sockaddr_in* addr4, IN const char* ipv4_or_dns, IN u_short port)
{
	int r;
	char portstr[16];
	struct addrinfo hints, *addr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
//	hints.ai_flags = AI_ADDRCONFIG;
	snprintf(portstr, sizeof(portstr), "%hu", port);
	r = getaddrinfo(ipv4_or_dns, portstr, &hints, &addr);
	if (0 != r)
		return r;

	// fixed ios getaddrinfo don't set port if node is ipv4 address
	socket_addr_setport(addr->ai_addr, (socklen_t)addr->ai_addrlen, port);
	assert(sizeof(struct sockaddr_in) == addr->ai_addrlen);
	memcpy(addr4, addr->ai_addr, addr->ai_addrlen);
	freeaddrinfo(addr);
	return 0;
}

static inline int socket_addr_from_ipv6(OUT struct sockaddr_in6* addr6, IN const char* ipv6_or_dns, IN u_short port)
{
	int r;
	char portstr[16];
	struct addrinfo hints, *addr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_flags = AI_V4MAPPED /*| AI_ADDRCONFIG*/; // AI_ADDRCONFIG linux "ff00::" return -2
	snprintf(portstr, sizeof(portstr), "%hu", port);
	r = getaddrinfo(ipv6_or_dns, portstr, &hints, &addr);
	if (0 != r)
		return r;

	// fixed ios getaddrinfo don't set port if node is ipv4 address
	socket_addr_setport(addr->ai_addr, (socklen_t)addr->ai_addrlen, port);
	assert(sizeof(struct sockaddr_in6) == addr->ai_addrlen);
	memcpy(addr6, addr->ai_addr, addr->ai_addrlen);
	freeaddrinfo(addr);
	return 0;
}

static inline int socket_addr_from(OUT struct sockaddr_storage* ss, OUT socklen_t* len, IN const char* ipv4_or_ipv6_or_dns, IN u_short port)
{
	int r;
	char portstr[16];
	struct addrinfo *addr;
	snprintf(portstr, sizeof(portstr), "%hu", port);
	r = getaddrinfo(ipv4_or_ipv6_or_dns, portstr, NULL, &addr);
	if (0 != r)
		return r;

	// fixed ios getaddrinfo don't set port if node is ipv4 address
	socket_addr_setport(addr->ai_addr, (socklen_t)addr->ai_addrlen, port);
	assert(addr->ai_addrlen <= sizeof(struct sockaddr_storage));
	memcpy(ss, addr->ai_addr, addr->ai_addrlen);
	if(len) *len = (socklen_t)addr->ai_addrlen;
	freeaddrinfo(addr);
	return 0;
}

static inline int socket_addr_to(IN const struct sockaddr* sa, IN socklen_t salen, OUT char ip[SOCKET_ADDRLEN], OUT u_short* port)
{
	if (AF_INET == sa->sa_family)
	{
		struct sockaddr_in* in = (struct sockaddr_in*)sa;
		assert(sizeof(struct sockaddr_in) == salen);
		inet_ntop(AF_INET, &in->sin_addr, ip, SOCKET_ADDRLEN);
		if(port) *port = ntohs(in->sin_port);
	}
	else if (AF_INET6 == sa->sa_family)
	{
		struct sockaddr_in6* in6 = (struct sockaddr_in6*)sa;
		assert(sizeof(struct sockaddr_in6) == salen);
		inet_ntop(AF_INET6, &in6->sin6_addr, ip, SOCKET_ADDRLEN);
		if (port) *port = ntohs(in6->sin6_port);
	}
	else
	{
		return -1; // unknown address family
	}

	return 0;
}

static inline int socket_addr_setport(IN struct sockaddr* sa, IN socklen_t salen, u_short port)
{
	if (AF_INET == sa->sa_family)
	{
		struct sockaddr_in* in = (struct sockaddr_in*)sa;
		assert(sizeof(struct sockaddr_in) == salen);
		in->sin_port = htons(port);
	}
	else if (AF_INET6 == sa->sa_family)
	{
		struct sockaddr_in6* in6 = (struct sockaddr_in6*)sa;
		assert(sizeof(struct sockaddr_in6) == salen);
		in6->sin6_port = htons(port);
	}
	else
	{
		assert(0);
		return -1;
	}

	return 0;
}

static inline int socket_addr_name(IN const struct sockaddr* sa, IN socklen_t salen, OUT char* host, IN socklen_t hostlen)
{
	return getnameinfo(sa, salen, host, hostlen, NULL, 0, 0);
}

static inline int socket_addr_is_multicast(IN const struct sockaddr* sa, IN socklen_t salen)
{
	if (AF_INET == sa->sa_family)
	{
		// 224.x.x.x ~ 239.x.x.x
		// b1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
		const struct sockaddr_in* in = (const struct sockaddr_in*)sa;
		assert(sizeof(struct sockaddr_in) == salen);
		return (ntohl(in->sin_addr.s_addr) & 0xf0000000) == 0xe0000000 ? 1 : 0;
	}
	else if (AF_INET6 == sa->sa_family)
	{
		// FFxx::/8
		const struct sockaddr_in6* in6 = (const struct sockaddr_in6*)sa;
		assert(sizeof(struct sockaddr_in6) == salen);
		return in6->sin6_addr.s6_addr[0] == 0xff ? 1 : 0;
	}
	else
	{
		assert(0);
	}

	return 0;
}

/// RECOMMAND: compare with struct sockaddr_storage
/// @return 0-equal, other-don't equal
static inline int socket_addr_compare(const struct sockaddr* sa, const struct sockaddr* sb)
{
	if(sa->sa_family != sb->sa_family)
		return sa->sa_family - sb->sa_family;

	// https://opensource.apple.com/source/postfix/postfix-197/postfix/src/util/sock_addr.c
	switch (sa->sa_family)
	{
	case AF_INET:
		return ((struct sockaddr_in*)sa)->sin_port==((struct sockaddr_in*)sb)->sin_port 
			&& 0 == memcmp(&((struct sockaddr_in*)sa)->sin_addr, &((struct sockaddr_in*)sb)->sin_addr, sizeof(struct in_addr))
			? 0 : -1;
	case AF_INET6:
		return ((struct sockaddr_in6*)sa)->sin6_port == ((struct sockaddr_in6*)sb)->sin6_port 
			&& 0 == memcmp(&((struct sockaddr_in6*)sa)->sin6_addr, &((struct sockaddr_in6*)sb)->sin6_addr, sizeof(struct in6_addr))
			? 0 : -1;

#if defined(OS_LINUX) || defined(OS_MAC) // Windows build 17061
	// https://blogs.msdn.microsoft.com/commandline/2017/12/19/af_unix-comes-to-windows/
	case AF_UNIX:	return memcmp(sa, sb, sizeof(struct sockaddr_un));
#endif
	default:		return -1;
	}
}

static inline int socket_addr_len(const struct sockaddr* addr)
{
	switch (addr->sa_family)
	{
	case AF_INET:	return sizeof(struct sockaddr_in);
	case AF_INET6:	return sizeof(struct sockaddr_in6);
#if defined(OS_LINUX) || defined(OS_MAC)// Windows build 17061
		// https://blogs.msdn.microsoft.com/commandline/2017/12/19/af_unix-comes-to-windows/
	case AF_UNIX:	return sizeof(struct sockaddr_un);
#endif
#if defined(AF_NETLINK)
	//case AF_NETLINK:return sizeof(struct sockaddr_nl);
#endif
	default: return 0;
	}
}

static inline void socket_setbufvec(INOUT socket_bufvec_t* vec, IN int idx, IN void* ptr, IN size_t len)
{
#if defined(OS_WINDOWS)
	vec[idx].buf = (CHAR*)ptr;
	vec[idx].len = (ULONG)len;
#else
	vec[idx].iov_base = ptr;
	vec[idx].iov_len = len;
#endif
}

static inline void socket_getbufvec(IN const socket_bufvec_t* vec, IN int idx, OUT void** ptr, OUT size_t* len)
{
#if defined(OS_WINDOWS)
	*ptr = (void*)vec[idx].buf;
	*len = (size_t)vec[idx].len;
#else
	*ptr = vec[idx].iov_base;
	*len = vec[idx].iov_len;
#endif
}

static inline int socket_multicast_join(IN socket_t sock, IN const char* group, IN const char* local)
{
#if defined(OS_WINDOWS)
	struct ip_mreq imr;
	memset(&imr, 0, sizeof(imr));
	inet_pton(AF_INET, group, &imr.imr_multiaddr);
	inet_pton(AF_INET, local, &imr.imr_interface);
	return setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&imr, sizeof(imr));
#else
	struct ip_mreqn imr;
	memset(&imr, 0, sizeof(imr));
	imr.imr_ifindex = 0; // any interface
	inet_pton(AF_INET, local, &imr.imr_address);
	inet_pton(AF_INET, group, &imr.imr_multiaddr);
	return setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&imr, sizeof(imr));
#endif
}

static inline int socket_multicast_leave(IN socket_t sock, IN const char* group, IN const char* local)
{
#if defined(OS_WINDOWS)
	struct ip_mreq imr;
	memset(&imr, 0, sizeof(imr));
	inet_pton(AF_INET, group, &imr.imr_multiaddr);
	inet_pton(AF_INET, local, &imr.imr_interface);
	return setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&imr, sizeof(imr));
#else
	struct ip_mreqn imr;
	memset(&imr, 0, sizeof(imr));
	imr.imr_ifindex = 0; // any interface
    inet_pton(AF_INET, local, &imr.imr_address);
    inet_pton(AF_INET, group, &imr.imr_multiaddr);
	return setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *) &imr, sizeof(imr));
#endif
}

static inline int socket_multicast_join6(IN socket_t sock, IN const char* group)
{
    struct ipv6_mreq imr;
    memset(&imr, 0, sizeof(imr));
    inet_pton(AF_INET6, group, &imr.ipv6mr_multiaddr);
    imr.ipv6mr_interface = 0;
    return setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *) &imr, sizeof(imr));
}

static inline int socket_multicast_leave6(IN socket_t sock, IN const char* group)
{
    struct ipv6_mreq imr;
    memset(&imr, 0, sizeof(imr));
    inet_pton(AF_INET6, group, &imr.ipv6mr_multiaddr);
    imr.ipv6mr_interface = 0;
    return setsockopt(sock, IPPROTO_IPV6, IPV6_LEAVE_GROUP, (char *) &imr, sizeof(imr));
}

static inline int socket_multicast_join_source(IN socket_t sock, IN const char* group, IN const char* source, IN const char* local)
{
	struct ip_mreq_source imr;
	memset(&imr, 0, sizeof(imr));
	inet_pton(AF_INET, source, &imr.imr_sourceaddr);
	inet_pton(AF_INET, group, &imr.imr_multiaddr);
	inet_pton(AF_INET, local, &imr.imr_interface);
	return setsockopt(sock, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, (char *) &imr, sizeof(imr));
}

static inline int socket_multicast_leave_source(IN socket_t sock, IN const char* group, IN const char* source, IN const char* local)
{
	struct ip_mreq_source imr;
	memset(&imr, 0, sizeof(imr));
	inet_pton(AF_INET, source, &imr.imr_sourceaddr);
	inet_pton(AF_INET, group, &imr.imr_multiaddr);
	inet_pton(AF_INET, local, &imr.imr_interface);
	return setsockopt(sock, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP, (char *)&imr, sizeof(imr));
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif /* !_platform_socket_h_ */
