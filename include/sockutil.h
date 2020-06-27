#ifndef _sockutil_h_
#define _sockutil_h_

#include "sys/sock.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#if defined(OS_WINDOWS)
#define SOCKET_TIMEDOUT -WSAETIMEDOUT
#define iov_base buf  
#define iov_len  len 
#else
#define SOCKET_TIMEDOUT -ETIMEDOUT
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 6031) // warning C6031: Return value ignored: 'snprintf'
#endif

/// @param[in] backlog Maximum queue length specifiable by listen, use SOMAXCONN if you don't known how to choose value
/// @param[in] ipv4 0-ipv6 only, 1-ipv6 dual stack
/// @return >=0-socket, <0-socket_error(by socket_geterror())
static inline socket_t socket_tcp_listen_ipv6(IN const char* ipv4_or_ipv6_or_dns, IN u_short port, IN int backlog, IN int ipv4);
static inline socket_t socket_tcp_listen(IN const char* ipv4_or_dns, IN u_short port, IN int backlog);
static inline socket_t socket_udp_bind(IN const char* ipv4_or_dns, IN u_short port);
static inline socket_t socket_udp_bind_ipv6(IN const char* ipv4_or_ipv6_or_dns, IN u_short port, IN int ipv4);

/// @return 0-ok, <0-socket_error(by socket_geterror())
static inline int socket_udp_multicast(IN socket_t sock, IN const char* group, IN const char* source, IN int ttl);

/// @Notice: need restore block status
/// @param[in] timeout ms, <0-forever
/// @return 0-ok, other-error code
static inline int socket_connect_by_time(IN socket_t sock, IN const struct sockaddr* addr, IN socklen_t addrlen, IN int timeout);
/// @return >=0-socket, <0-socket_invalid(by socket_geterror())
static inline socket_t socket_connect_host(IN const char* ipv4_or_ipv6_or_dns, IN u_short port, IN int timeout); // timeout: ms, <0-forever
static inline socket_t socket_accept_by_time(IN socket_t socket, OUT struct sockaddr_storage* addr, OUT socklen_t* addrlen, IN int timeout); // timeout: <0-forever

/// @return 0-ok, <0-socket_error(by socket_geterror())
static inline int socket_bind_any(IN socket_t sock, IN u_short port);
static inline int socket_bind_any_ipv4(IN socket_t sock, IN u_short port);
static inline int socket_bind_any_ipv6(IN socket_t sock, IN u_short port);

/// @return >0-sent/received bytes, SOCKET_TIMEDOUT-timeout, <0-error(by socket_geterror()), 0-connection closed(recv only)
static inline int socket_send_by_time(IN socket_t sock, IN const void* buf, IN size_t len, IN int flags, IN int timeout); // timeout: ms, <0-forever
static inline int socket_send_all_by_time(IN socket_t sock, IN const void* buf, IN size_t len, IN int flags, IN int timeout); // timeout: ms, <0-forever
static inline int socket_recv_by_time(IN socket_t sock, OUT void* buf, IN size_t len, IN int flags, IN int timeout); // timeout: ms, <0-forever
static inline int socket_recv_all_by_time(IN socket_t sock, OUT void* buf, IN size_t len, IN int flags, IN int timeout);  // timeout: ms, <0-forever
static inline int socket_send_v_all_by_time(IN socket_t sock, IN socket_bufvec_t* vec, IN int n, IN int flags, IN int timeout); // timeout: ms, <0-forever

/// @param[out] local ip destination addr(local address) without port
static inline int socket_recvfrom_addr(IN socket_t sock, OUT socket_bufvec_t* vec, IN int n, IN int flags, OUT struct sockaddr* peer, OUT socklen_t* peerlen, OUT struct sockaddr* local, OUT socklen_t* locallen);
static inline int socket_sendto_addr(IN socket_t sock, IN const socket_bufvec_t* vec, IN int n, IN int flags, IN const struct sockaddr* peer, IN socklen_t peerlen, IN const struct sockaddr* local, IN socklen_t locallen);

static inline int64_t socket_poll_read(socket_t s[], int n, int timeout);
static inline int64_t socket_poll_readv(int timeout, int n, ...);

//////////////////////////////////////////////////////////////////////////
/// socket connect
//////////////////////////////////////////////////////////////////////////

/// @Notice: need restore block status
/// @param[in] timeout: <0-forever
/// @return 0-ok, other-error(by socket_geterror())
static inline int socket_connect_by_time(IN socket_t sock, IN const struct sockaddr* addr, IN socklen_t addrlen, IN int timeout)
{
	int r;
	socket_setnonblock(sock, 1);
	r = socket_connect(sock, addr, addrlen);
	assert(r <= 0);
#if defined(OS_WINDOWS)
	if (0 != r && WSAEWOULDBLOCK == WSAGetLastError())
#else
	if (0 != r && EINPROGRESS == errno)
#endif
	{
		r = socket_select_connect(sock, timeout);
	}

	// r = socket_setnonblock(sock, 0);
	return r;
}

/// @param[in] timeout ms, -1==infinite
static inline socket_t socket_connect_host(IN const char* ipv4_or_ipv6_or_dns, IN u_short port, IN int timeout)
{
	int r;
	socket_t sock;
	char portstr[16];
	struct addrinfo hints, *addr, *ptr;

	memset(&hints, 0, sizeof(hints));
//	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
//	hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
	snprintf(portstr, sizeof(portstr), "%hu", port);
	r = getaddrinfo(ipv4_or_ipv6_or_dns, portstr, &hints, &addr);
	if (0 != r)
		return socket_invalid;

	r = -1; // not found
    sock = socket_invalid;
	for (ptr = addr; 0 != r && ptr != NULL; ptr = ptr->ai_next)
	{
		sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (socket_invalid == sock)
			continue;

		// fixed ios getaddrinfo don't set port if nodename is ipv4 address
		socket_addr_setport(ptr->ai_addr, (socklen_t)ptr->ai_addrlen, port);

		if (timeout < 0)
			r = socket_connect(sock, ptr->ai_addr, (socklen_t)ptr->ai_addrlen);
		else
			r = socket_connect_by_time(sock, ptr->ai_addr, (socklen_t)ptr->ai_addrlen, timeout);

		if (0 != r)
			socket_close(sock);
	}

	freeaddrinfo(addr);
	return 0 == r ? sock : socket_invalid;
}


//////////////////////////////////////////////////////////////////////////
/// socket bind
//////////////////////////////////////////////////////////////////////////
static inline int socket_bind_any_ipv4(IN socket_t sock, IN u_short port)
{
    struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    return socket_bind(sock, (struct sockaddr*)&addr, sizeof(addr));
}

static inline int socket_bind_any_ipv6(IN socket_t sock, IN u_short port)
{
    struct sockaddr_in6 addr6;
    memset(&addr6, 0, sizeof(addr6));
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons(port);
    addr6.sin6_addr = in6addr_any;
    return socket_bind(sock, (struct sockaddr*)&addr6, sizeof(addr6));
}

static inline int socket_bind_any(IN socket_t sock, IN u_short port)
{
	int r;
	int domain;
	r = socket_getdomain(sock, &domain);
	if (0 != r)
		return r;

	if (AF_INET == domain)
	{
        return socket_bind_any_ipv4(sock, port);
	}
	else if (AF_INET6 == domain)
	{
        return socket_bind_any_ipv6(sock, port);
	}
	else
	{
		assert(0);
		return socket_error;
	}
}

//////////////////////////////////////////////////////////////////////////
/// TCP/UDP server socket
//////////////////////////////////////////////////////////////////////////

/// TCP/UDP socket bind to address(IPv4/IPv6)
/// @param[in] socktype SOCK_DGRAM/SOCK_STREAM
static inline socket_t socket_bind_addr(const struct sockaddr* addr, int socktype)
{
	socket_t s;

	s = socket(addr->sa_family, socktype, 0);
	if (socket_invalid == s)
		return socket_invalid;

	if (0 != socket_bind(s, addr, socket_addr_len(addr)))
	{
		socket_close(s);
		return socket_invalid;
	}

	return s;
}

/// create a new TCP socket, bind, and listen
/// @param[in] ipv4_or_dns socket bind local address, NULL-bind any address
/// @param[in] port bind local port
/// @param[in] backlog the maximum length to which the queue of pending connections for socket may grow
/// @return socket_invalid-error, use socket_geterror() to get error code, other-ok 
static inline socket_t socket_tcp_listen(IN const char* ipv4_or_dns, IN u_short port, IN int backlog)
{
	int r;
	socket_t sock;
	char portstr[22];
	struct addrinfo hints, *addr, *ptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4 only
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
	snprintf(portstr, sizeof(portstr), "%hu", port);
	r = getaddrinfo(ipv4_or_dns, portstr, &hints, &addr);
	if (0 != r)
		return socket_invalid;

	r = -1; // not found
    sock = socket_invalid;
	for (ptr = addr; 0 != r && ptr != NULL; ptr = ptr->ai_next)
	{
		assert(AF_INET == ptr->ai_family);
		sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (socket_invalid == sock)
			continue;

		// reuse addr
		socket_setreuseaddr(sock, 1);

		// fixed ios getaddrinfo don't set port if nodename is ipv4 address
		socket_addr_setport(ptr->ai_addr, (socklen_t)ptr->ai_addrlen, port);

		r = socket_bind(sock, ptr->ai_addr, (socklen_t)ptr->ai_addrlen);
		if (0 == r)
			r = socket_listen(sock, backlog);

		if (0 != r)
			socket_close(sock);
	}

	freeaddrinfo(addr);
	return 0 == r ? sock : socket_invalid;
}

/// create a new TCP socket, bind, and listen
/// @param[in] ipv4_or_ipv6_or_dns socket bind local address, NULL-bind any address
/// @param[in] port bind local port
/// @param[in] backlog the maximum length to which the queue of pending connections for socket may grow
/// @param[in] ipv4 0-ipv6 only, 1-ipv6 dual stack
/// @return socket_invalid-error, use socket_geterror() to get error code, other-ok 
static inline socket_t socket_tcp_listen_ipv6(IN const char* ipv4_or_ipv6_or_dns, IN u_short port, IN int backlog, IN int ipv4)
{
	int r;
	socket_t sock;
	char portstr[22];
	struct addrinfo hints, *addr, *ptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_V4MAPPED;
	snprintf(portstr, sizeof(portstr), "%hu", port);
	r = getaddrinfo(ipv4_or_ipv6_or_dns, portstr, &hints, &addr);
	if (0 != r)
		return socket_invalid;

	r = -1; // not found
    sock = socket_invalid;
	for (ptr = addr; 0 != r && ptr != NULL; ptr = ptr->ai_next)
	{
		assert(AF_INET6 == ptr->ai_family);
		sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (socket_invalid == sock)
			continue;

		// Dual-Stack Socket Option
		// https://msdn.microsoft.com/en-us/library/windows/desktop/bb513665(v=vs.85).aspx
		// By default, an IPv6 socket created on Windows Vista and later only operates over the IPv6 protocol.
		if (0 != socket_setreuseaddr(sock, 1) || 0 != socket_setipv6only(sock, ipv4 ? 0 : 1))
		{
			socket_close(sock);
			continue;
		}

		// fixed ios getaddrinfo don't set port if nodename is ipv4 address
		socket_addr_setport(ptr->ai_addr, (socklen_t)ptr->ai_addrlen, port);

		r = socket_bind(sock, ptr->ai_addr, (socklen_t)ptr->ai_addrlen);
		if (0 == r)
			r = socket_listen(sock, backlog);

		if (0 != r)
			socket_close(sock);
	}

	freeaddrinfo(addr);
	return 0 == r ? sock : socket_invalid;
}

/// @param[in] reuse 1-enable reuse addr
/// @param[in] dual 1-enable ipv6 dual stack
/// @return socket_invalid-error, other-ok
static inline socket_t socket_udp_bind_addr(IN const struct sockaddr* addr, int reuse, int dual)
{
	socket_t s;
	
	assert(AF_INET == addr->sa_family || AF_INET6 == addr->sa_family);
	s = socket(addr->sa_family, SOCK_DGRAM, 0);
	if (socket_invalid == s)
		return socket_invalid;

	// disable reuse addr(fixed rtp port reuse error)
	if (reuse && 0 != socket_setreuseaddr(s, 1))
	{
		socket_close(s);
		return socket_invalid;
	}

	// Dual-Stack Socket option
	// https://msdn.microsoft.com/en-us/library/windows/desktop/bb513665(v=vs.85).aspx
	// By default, an IPv6 socket created on Windows Vista and later only operates over the IPv6 protocol.
	if (AF_INET6 == addr->sa_family && 0 != socket_setipv6only(s, dual ? 1 : 0))
	{
		socket_close(s);
		return socket_invalid;
	}

	if (0 != socket_bind(s, addr, socket_addr_len(addr)))
	{
		socket_close(s);
		return socket_invalid;
	}

	return s;
}

/// create a new UDP socket and bind with ip/port
/// @param[in] ipv4_or_ipv6_or_dns socket bind local address, NULL-bind any address
/// @param[in] port bind local port
/// @return socket_invalid-error, use socket_geterror() to get error code, other-ok 
static inline socket_t socket_udp_bind(IN const char* ipv4_or_ipv6_or_dns, IN u_short port)
{
	socket_t sock;
	char portstr[16];
	struct addrinfo hints, *addr, *ptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4 only
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
	snprintf(portstr, sizeof(portstr), "%hu", port);
	if (0 != getaddrinfo(ipv4_or_ipv6_or_dns, portstr, &hints, &addr))
		return socket_invalid;

    sock = socket_invalid;
	for (ptr = addr; socket_invalid == sock && ptr != NULL; ptr = ptr->ai_next)
	{
		assert(AF_INET == ptr->ai_family);
		
		// fixed ios getaddrinfo don't set port if nodename is ipv4 address
		socket_addr_setport(ptr->ai_addr, (socklen_t)ptr->ai_addrlen, port);
		
		sock = socket_udp_bind_addr(ptr->ai_addr, 0, 0);
	}

	freeaddrinfo(addr);
	return sock;
}

/// create a new UDP socket and bind with ip/port
/// @param[in] ipv4_or_ipv6_or_dns socket bind local address, NULL-bind any address
/// @param[in] port bind local port
/// @param[in] ipv4 0-ipv6 only, 1-ipv6 dual stack
/// @return socket_invalid-error, use socket_geterror() to get error code, other-ok 
static inline socket_t socket_udp_bind_ipv6(IN const char* ipv4_or_ipv6_or_dns, IN u_short port, IN int ipv4)
{
	socket_t sock;
	char portstr[16];
	struct addrinfo hints, *addr, *ptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
	snprintf(portstr, sizeof(portstr), "%hu", port);
	if (0 != getaddrinfo(ipv4_or_ipv6_or_dns, portstr, &hints, &addr))
		return socket_invalid;

    sock = socket_invalid;
	for (ptr = addr; socket_invalid == sock && ptr != NULL; ptr = ptr->ai_next)
	{
		assert(AF_INET6 == ptr->ai_family);

		// fixed ios getaddrinfo don't set port if nodename is ipv4 address
		socket_addr_setport(ptr->ai_addr, (socklen_t)ptr->ai_addrlen, port);

		sock = socket_udp_bind_addr(ptr->ai_addr, 0, ipv4);
	}

	freeaddrinfo(addr);
	return sock;
}

static inline int socket_udp_multicast(IN socket_t sock, IN const char* group, IN const char* source, IN int ttl)
{
    int r;
    int domain;
    u_short port;
    char local[SOCKET_ADDRLEN];

    r = socket_getdomain(sock, &domain);
    if(AF_INET == domain)
    {
        r = r ? r : socket_getname(sock, local, &port);
        r = r ? r : socket_setopt_bool(sock, IP_MULTICAST_LOOP, 0); // disable Loop
        r = r ? r : socket_setopt_bool(sock, IP_MULTICAST_TTL, ttl <= 0 ? 1 : ttl); // ttl default 1
        // r = r ? r : setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &imr.imr_interface.s_addr, sizeof(struct in_addr)); // bind to interface
        if(source && *source)
            r = r ? r : socket_multicast_join_source(sock, group, source, local);
        else
            r = r ? r : socket_multicast_join(sock, group, local);
        //socket_multicast_leave_source(sock, group, source, local);
    }
    else if(AF_INET6 == domain)
    {
        r = r ? r : socket_setopt_bool(sock, IPV6_MULTICAST_LOOP, 0); // disable Loop
        r = r ? r : socket_setopt_bool(sock, IPV6_MULTICAST_HOPS, ttl <= 0 ? 1 : ttl); // ttl default 1
        r = r ? r : socket_multicast_join6(sock, group);
    }
    else
    {
        assert(0);
        r = -1;
    }
    
    return r;
}

/// wait for client connection
/// @param[in] socket server socket(must be bound and listening)
/// @param[out] addr struct sockaddr_in for IPv4
/// @param[out] addrlen addr length in bytes
/// @param[in] timeout timeout in millisecond
/// @return socket_invalid-error, use socket_geterror() to get error code, other-ok  
static inline socket_t socket_accept_by_time(IN socket_t socket, OUT struct sockaddr_storage* addr, OUT socklen_t* addrlen, IN int timeout)
{
	int ret;
	
	assert(socket_invalid != socket);
	ret = socket_select_read(socket, timeout);
	if (socket_error == ret)
	{
		return socket_invalid;
	}
	else if (0 == ret)
	{
		return socket_invalid; //SOCKET_TIMEDOUT
	}

	return socket_accept(socket, addr, addrlen);
}


//////////////////////////////////////////////////////////////////////////
/// send/recv by time
//////////////////////////////////////////////////////////////////////////

/// @param[in] timeout ms, -1==infinite
/// @return >0-sent bytes, SOCKET_TIMEDOUT-timeout, <0-error(by socket_geterror())
static inline int socket_send_by_time(IN socket_t sock, IN const void* buf, IN size_t len, IN int flags, IN int timeout)
{
	int r;

	r = socket_select_write(sock, timeout);
	if (r <= 0)
		return 0 == r ? SOCKET_TIMEDOUT : r;
	assert(1 == r);

	r = socket_send(sock, buf, len, flags);
	return r;
}

/// @param[in] timeout ms, -1==infinite
/// @return >0-sent bytes, SOCKET_TIMEDOUT-timeout, <0-error(by socket_geterror())
static inline int socket_send_all_by_time(IN socket_t sock, IN const void* buf, IN size_t len, IN int flags, IN int timeout)
{
	int r;
	size_t bytes = 0;

	while (bytes < len)
	{
		r = socket_send_by_time(sock, (const char*)buf + bytes, len - bytes, flags, timeout);
		if (r <= 0)
			return r;	// <0-error

		bytes += r;
	}
	return (int)bytes;
}

/// @param[in] timeout ms, -1==infinite
/// @return >0-received bytes, SOCKET_TIMEDOUT-timeout, <0-error(by socket_geterror()), 0-connection closed(recv only)
static inline int socket_recv_by_time(IN socket_t sock, OUT void* buf, IN size_t len, IN int flags, IN int timeout)
{
	int r;

	r = socket_select_read(sock, timeout);
	if (r <= 0)
		return 0 == r ? SOCKET_TIMEDOUT : r;
	assert(1 == r);

	r = socket_recv(sock, buf, len, flags);
	return r;
}

/// @param[in] timeout ms, -1==infinite
/// @return >0-received bytes, SOCKET_TIMEDOUT-timeout, <0-error(by socket_geterror()), 0-connection closed(recv only)
static inline int socket_recv_all_by_time(IN socket_t sock, OUT void* buf, IN size_t len, IN int flags, IN int timeout)
{
	int r;
	size_t bytes = 0;

	while (bytes < len)
	{
		r = socket_recv_by_time(sock, (char*)buf + bytes, len - bytes, flags, timeout);
		if (r <= 0)
			return r;	// <0-error / 0-connection closed

		bytes += r;
	}
	return (int)bytes;
}

/// @param[in] timeout ms, -1==infinite
/// @return >0-sent bytes, SOCKET_TIMEDOUT-timeout, <0-error(by socket_geterror())
static inline int socket_send_v_all_by_time(IN socket_t sock, IN socket_bufvec_t* vec, IN int n, IN int flags, IN int timeout)
{
	int r, i;
	size_t count;
	size_t bytes = 0;

	while (n > 0)
	{
		r = socket_select_write(sock, timeout);
		if (r <= 0)
			return 0 == r ? SOCKET_TIMEDOUT : r;
		assert(1 == r);

		r = socket_send_v(sock, vec, n, flags);
		if (r <= 0)
			return r;	// <0-error

		bytes += r;

		for (i = 0, count = 0; i < n; i++)
		{
			if (count + vec[i].iov_len > (size_t)r)
				break;
			count += vec[i].iov_len;
		}

		n -= i;
		if (n > 0)
		{
			count = r - count;
			vec[i].iov_len -= count;
			vec[i].iov_base = (char*)vec[i].iov_base + count;
			vec += i;
		}
	}

	return (int)bytes;
}

static inline int socket_recvfrom_by_time(IN socket_t sock, OUT void* buf, IN size_t len, IN int flags, OUT struct sockaddr* from, OUT socklen_t* fromlen, IN int timeout)
{
    int r;

    r = socket_select_read(sock, timeout);
    if (r <= 0)
        return 0 == r ? SOCKET_TIMEDOUT : r;
    assert(1 == r);

    r = socket_recvfrom(sock, buf, len, flags, from, fromlen);
    return r;
}

/// @param[in] timeout ms, -1==infinite
/// @return >0-sent bytes, SOCKET_TIMEDOUT-timeout, <0-error(by socket_geterror())
static inline int socket_sendto_by_time(IN socket_t sock, IN const void* buf, IN size_t len, IN int flags, IN const struct sockaddr* to, IN socklen_t tolen, IN int timeout)
{
    int r;

    r = socket_select_write(sock, timeout);
    if (r <= 0)
        return 0 == r ? SOCKET_TIMEDOUT : r;
    assert(1 == r);

    r = socket_sendto(sock, buf, len, flags, to, tolen);
    return r;
}

static inline int socket_sendto_v_by_time(IN socket_t sock, IN const socket_bufvec_t* vec, IN int n, IN int flags, IN const struct sockaddr* to, IN socklen_t tolen, IN int timeout)
{
	int r;

	r = socket_select_write(sock, timeout);
	if (r <= 0)
		return 0 == r ? SOCKET_TIMEDOUT : r;
	assert(1 == r);

	r = socket_sendto_v(sock, vec, n, flags, to, tolen);
	return r;
}

#if defined(OS_WINDOWS) && _WIN32_WINNT >= 0x0600
#include <Mswsock.h>
static inline BOOL WINAPI wsarecvmsgcallback(PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Parameter2)
{
	DWORD bytes;
	socket_t sock;
	GUID guid = WSAID_WSARECVMSG;
	sock = socket_tcp();
	WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(GUID), Parameter, sizeof(LPFN_WSARECVMSG), &bytes, NULL, NULL);
	socket_close(sock);
	(void)InitOnce, (void)Parameter2;
	return TRUE;
}
#endif

/// @param[out] local ip destination addr(local address) without port
static inline int socket_recvfrom_addr(IN socket_t sock, OUT socket_bufvec_t* vec, IN int n, IN int flags, OUT struct sockaddr* peer, OUT socklen_t* peerlen, OUT struct sockaddr* local, OUT socklen_t* locallen)
{
	int r;
	char control[64];
	
#if defined(OS_WINDOWS) && _WIN32_WINNT >= 0x0600
	struct cmsghdr *cmsg;
	struct in_pktinfo* pktinfo;
	struct in6_pktinfo* pktinfo6;
	static INIT_ONCE wsarecvmsgonce;
	static LPFN_WSARECVMSG WSARecvMsg;

	DWORD bytes;
	WSAMSG wsamsg;
	InitOnceExecuteOnce(&wsarecvmsgonce, wsarecvmsgcallback, &WSARecvMsg, NULL);
	memset(control, 0, sizeof(control));
	memset(&wsamsg, 0, sizeof(wsamsg));
	wsamsg.name = peer;
	wsamsg.namelen = *peerlen;
	wsamsg.lpBuffers = vec;
	wsamsg.dwBufferCount = n;
	wsamsg.Control.buf = control;
	wsamsg.Control.len = sizeof(control);
	wsamsg.dwFlags = flags;
	r = WSARecvMsg(sock, &wsamsg, &bytes, NULL, NULL);
	if (0 != r)
		return r;

	*peerlen = wsamsg.namelen;
	for (cmsg = CMSG_FIRSTHDR(&wsamsg); !!cmsg && local && locallen; cmsg = CMSG_NXTHDR(&wsamsg, cmsg))
	{
		if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO && *locallen >= sizeof(struct sockaddr_in))
		{
			pktinfo = (struct in_pktinfo*)WSA_CMSG_DATA(cmsg);
			*locallen = sizeof(struct sockaddr_in);
			memset(local, 0, sizeof(struct sockaddr_in));
			((struct sockaddr_in*)local)->sin_family = AF_INET;
			memcpy(&((struct sockaddr_in*)local)->sin_addr, &pktinfo->ipi_addr, sizeof(pktinfo->ipi_addr));
			break;
		}
		else if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO && *locallen >= sizeof(struct sockaddr_in6))
		{
			pktinfo6 = (struct in6_pktinfo*)WSA_CMSG_DATA(cmsg);
			*locallen = sizeof(struct sockaddr_in6);
			memset(local, 0, sizeof(struct sockaddr_in6));
			((struct sockaddr_in6*)local)->sin6_family = AF_INET6;
			memcpy(&((struct sockaddr_in6*)local)->sin6_addr, &pktinfo6->ipi6_addr, sizeof(pktinfo6->ipi6_addr));
			break;
		}
	}

	return bytes;
	
#elif defined(OS_LINUX) || defined(OS_MAC)
	struct msghdr hdr;
	struct cmsghdr *cmsg;
	memset(&hdr, 0, sizeof(hdr));
	memset(control, 0, sizeof(control));
	hdr.msg_name = peer;
	hdr.msg_namelen = *peerlen;
	hdr.msg_iov = vec;
	hdr.msg_iovlen = n;
	hdr.msg_control = control;
	hdr.msg_controllen = sizeof(control);
	hdr.msg_flags = 0;
	r = (int)recvmsg(sock, &hdr, flags);
	if (-1 == r)
		return -1;

	*peerlen = hdr.msg_namelen;
	for (cmsg = CMSG_FIRSTHDR(&hdr); !!cmsg && local && locallen; cmsg = CMSG_NXTHDR(&hdr, cmsg))
	{
		if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO && *locallen >= sizeof(struct sockaddr_in))
		{
			struct in_pktinfo* pktinfo;
			pktinfo = (struct in_pktinfo*)CMSG_DATA(cmsg);
			*locallen = sizeof(struct sockaddr_in);
			memset(local, 0, sizeof(struct sockaddr_in));
			((struct sockaddr_in*)local)->sin_family = AF_INET;
			memcpy(&((struct sockaddr_in*)local)->sin_addr, &pktinfo->ipi_addr, sizeof(pktinfo->ipi_addr));
			break;
		}
#if defined(_GNU_SOURCE)
		else if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO && *locallen >= sizeof(struct sockaddr_in6))
		{
			struct in6_pktinfo* pktinfo6;
			pktinfo6 = (struct in6_pktinfo*)CMSG_DATA(cmsg);
			*locallen = sizeof(struct sockaddr_in6);
			memset(local, 0, sizeof(struct sockaddr_in6));
			((struct sockaddr_in6*)local)->sin6_family = AF_INET6;
			memcpy(&((struct sockaddr_in6*)local)->sin6_addr, &pktinfo6->ipi6_addr, sizeof(pktinfo6->ipi6_addr));
			break;
		}
#endif
	}

	return r;
#else
#pragma error("xxxx\n");
	return -1;
#endif
}

static inline int socket_sendto_addr(IN socket_t sock, IN const socket_bufvec_t* vec, IN int n, IN int flags, IN const struct sockaddr* peer, IN socklen_t peerlen, IN const struct sockaddr* local, IN socklen_t locallen)
{
	char control[64];

#if defined(OS_WINDOWS) && _WIN32_WINNT >= 0x0600
	struct cmsghdr *cmsg;
	struct in_pktinfo* pktinfo;
	struct in6_pktinfo* pktinfo6;

	DWORD bytes;
	WSAMSG wsamsg;
	wsamsg.name = (LPSOCKADDR)peer;
	wsamsg.namelen = peerlen;
	wsamsg.lpBuffers = (LPWSABUF)vec;
	wsamsg.dwBufferCount = n;
	wsamsg.Control.buf = control;
	wsamsg.Control.len = sizeof(control);
	wsamsg.dwFlags = 0;

	cmsg = CMSG_FIRSTHDR(&wsamsg);
	if (AF_INET == local->sa_family && locallen >= sizeof(struct sockaddr_in))
	{
		cmsg->cmsg_level = IPPROTO_IP;
		cmsg->cmsg_type = IP_PKTINFO;
		cmsg->cmsg_len = WSA_CMSG_LEN(sizeof(struct in_pktinfo));
		pktinfo = (struct in_pktinfo*)WSA_CMSG_DATA(cmsg);
		memset(pktinfo, 0, sizeof(struct in_pktinfo));
		memcpy(&pktinfo->ipi_addr, &((struct sockaddr_in*)local)->sin_addr, sizeof(pktinfo->ipi_addr));
		wsamsg.Control.len = CMSG_SPACE(sizeof(struct in_pktinfo));
	}
	else if (AF_INET6 == local->sa_family && locallen >= sizeof(struct sockaddr_in6))
	{
		cmsg->cmsg_level = IPPROTO_IPV6;
		cmsg->cmsg_type = IPV6_PKTINFO;
		cmsg->cmsg_len = WSA_CMSG_LEN(sizeof(struct in6_pktinfo));
		pktinfo6 = (struct in6_pktinfo*)WSA_CMSG_DATA(cmsg);
		memset(pktinfo6, 0, sizeof(struct in6_pktinfo));
		memcpy(&pktinfo6->ipi6_addr, &((struct sockaddr_in6*)local)->sin6_addr, sizeof(pktinfo6->ipi6_addr));
		wsamsg.Control.len = CMSG_SPACE(sizeof(struct in6_pktinfo));
	}
	else
	{
		assert(0);
		return -1;
	}

	return 0 == WSASendMsg(sock, &wsamsg, flags, &bytes, NULL, NULL) ? bytes : SOCKET_ERROR;

#elif defined(OS_LINUX) || defined(OS_MAC)
	struct msghdr hdr;
	struct cmsghdr *cmsg;

	memset(&hdr, 0, sizeof(hdr));
	memset(control, 0, sizeof(control));
	hdr.msg_name = (void*)peer;
	hdr.msg_namelen = peerlen;
	hdr.msg_iov = (struct iovec*)vec;
	hdr.msg_iovlen = n;
	hdr.msg_control = control;
	hdr.msg_controllen = sizeof(control);
	hdr.msg_flags = 0;

	cmsg = CMSG_FIRSTHDR(&hdr);
	if (AF_INET == local->sa_family)
	{
		struct in_pktinfo* pktinfo;
        cmsg->cmsg_level = IPPROTO_IP; // SOL_IP
		cmsg->cmsg_type = IP_PKTINFO;
		cmsg->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));
		pktinfo = (struct in_pktinfo*)CMSG_DATA(cmsg);
		memset(pktinfo, 0, sizeof(struct in_pktinfo));
		memcpy(&pktinfo->ipi_spec_dst, &((struct sockaddr_in*)local)->sin_addr, sizeof(pktinfo->ipi_spec_dst));
		hdr.msg_controllen = CMSG_SPACE(sizeof(struct in_pktinfo));
	}
#if defined(_GNU_SOURCE)
	else if (AF_INET6 == local->sa_family)
	{
		struct in6_pktinfo* pktinfo6;
		cmsg->cmsg_level = IPPROTO_IPV6;
		cmsg->cmsg_type = IPV6_PKTINFO;
		cmsg->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
		pktinfo6 = (struct in6_pktinfo*)CMSG_DATA(cmsg);
		memset(pktinfo6, 0, sizeof(struct in6_pktinfo));
		memcpy(&pktinfo6->ipi6_addr, &((struct sockaddr_in6*)local)->sin6_addr, sizeof(pktinfo6->ipi6_addr));
		hdr.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));
	}
#endif
	else
	{
		assert(0);
		return -1;
	}

	return (int)sendmsg(sock, &hdr, flags);
#else
#pragma error("xxxx\n");
	return -1;
#endif
}

/// @param[in] n total socket number, [1 ~ 64]
/// @return <0-error, =0-timeout, >0-socket bitmask
static inline int64_t socket_poll_read(socket_t s[], int n, int timeout)
{
	int i;
	int64_t r;

#if defined(OS_WINDOWS)
	fd_set rfds;
	fd_set wfds;
	fd_set efds;
	struct timeval tv;

	assert(n <= 64);
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);
	for (i = 0; i < n && i < 64; i++)
	{
		if(socket_invalid != s[i])
			FD_SET(s[i], &rfds);
	}

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;
	r = select(n, &rfds, &wfds, &efds, timeout < 0 ? NULL : &tv);
	if (r <= 0)
		return r;

	for (r = i = 0; i < n && i < 64; i++)
	{
		if (socket_invalid == s[i])
			continue;
		if (FD_ISSET(s[i], &rfds))
			r |= (int64_t)1 << i;
	}

	return r;
#else
	int j;
	struct pollfd fds[64];
	assert(n <= 64);
	for (j = i = 0; i < n && i < 64; i++)
	{
		if (socket_invalid == s[i])
			continue;
		fds[j].fd = s[i];
		fds[j].events = POLLIN;
		fds[j].revents = 0;
		j++;
	}

	r = poll(fds, j, timeout);
	while (-1 == r && (EINTR == errno || EAGAIN == errno))
		r = poll(fds, j, timeout);

	for (r = i = 0; i < n && i < 64; i++)
	{
		if (socket_invalid == s[i])
			continue;
		if (fds[i].revents & POLLIN)
			r |= (int64_t)1 << i;
	}

	return r;
#endif
}

static inline int64_t socket_poll_readv(int timeout, int n, ...)
{
	int i;
	va_list args;
	socket_t fd[64];

	assert(n <= 64 && 64 <= FD_SETSIZE);
	va_start(args, n);
	for (i = 0; i < n && i < 64; i++)
		fd[i] = va_arg(args, socket_t);
	va_end(args);

	return socket_poll_read(fd, n, timeout);
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif /* !_sockutil_h_ */
