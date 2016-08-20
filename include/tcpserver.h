#ifndef _tcpserver_h_
#define _tcpserver_h_

#include "sys/sock.h"

/// create a new TCP socket, bind, and listen
/// @param[in] ip socket bind local address, NULL-bind any address
/// @param[in] port bind local port
/// @param[in] backlog the maximum length to which the queue of pending connections for socket may grow
/// @return socket_invalid-error, use socket_geterror() to get error code, other-ok 
inline socket_t tcpserver_create(const char* ip, int port, int backlog)
{
	int r;
	socket_t sock;
	char portstr[16];
	struct addrinfo hints, *addr, *ptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
	snprintf(portstr, sizeof(portstr), "%d", port);
	r = getaddrinfo(ip, portstr, &hints, &addr);
	if (0 != r)
		return socket_invalid;

	r = -1; // not found
	for (ptr = addr; 0 != r && ptr != NULL; ptr = ptr->ai_next)
	{
#if !defined(IPV6)
		if(AF_INET6 == ptr->ai_family)
			continue;
#endif
		sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (socket_invalid == socket)
			continue;

		// reuse addr
		socket_setreuseaddr(sock, 1);

		// restrict IPv6 only
#if defined(OS_LINUX)
		if(AF_INET6 == ptr->ai_addr->sa_family)
			socket_setipv6only(sock, 1);
#endif

		r = socket_bind(sock, ptr->ai_addr, ptr->ai_addrlen);
		if(0 == r)
			r = socket_listen(sock, backlog);

		if (0 != r)
			socket_close(sock);
	}

	freeaddrinfo(addr);
	return 0 == r ? sock : socket_invalid;
}

/// wait for client connection
/// @param[in] socket server socket(must be bound and listening)
/// @param[out] addr struct sockaddr_in for IPv4
/// @param[out] addrlen addr length in bytes
/// @param[in] mstimeout timeout in millisecond
/// @return 0-timeout, socket_invalid-error, use socket_geterror() to get error code, other-ok  
inline socket_t tcpserver_accept(socket_t socket, struct sockaddr_storage* addr, socklen_t* addrlen, int mstimeout)
{
	int ret;
	socket_t client;

	assert(socket_invalid != 0);
	ret = socket_select_read(socket, mstimeout);
	if(socket_error == ret)
	{
		return socket_invalid;
	}
	else if(0 == ret)
	{
		return 0; // timeout
	}

	client = socket_accept(socket, addr, addrlen);
	if(socket_invalid == client)
		return socket_invalid;

	return client;
}

#endif /* !_tcpserver_h_ */
