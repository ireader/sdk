#ifndef _udpsocket_h_
#define _udpsocket_h_

#include "sys/sock.h"

inline socket_t udpsocket_create(const char* ip, int port)
{
	int r;
	socket_t sock;
	char portstr[16];
	struct addrinfo hints, *addr, *ptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
	sprintf(portstr, "%d", port);
	r = getaddrinfo(ip, portstr, &hints, &addr);
	if (0 != r)
		return socket_invalid;

	r = -1; // not found
	for (ptr = addr; 0 != r && ptr != NULL; ptr = ptr->ai_next)
	{
#if !defined(IPV6)
		if (AF_INET6 == ptr->ai_family)
			continue;
#endif
		sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (socket < 0)
			continue;

		// reuse addr
//		socket_setreuseaddr(sock, 1);

		// restrict IPv6 only
#if defined(OS_LINUX)
		if (AF_INET6 == ptr->ai_addr->sa_family)
			socket_setipv6only(sock, 1);
#endif

		r = socket_bind(sock, ptr->ai_addr, ptr->ai_addrlen);
		if (0 != r)
			socket_close(sock);
	}

	freeaddrinfo(addr);
	return 0 == r ? sock : socket_invalid;
}

#endif /* !_udpsocket_h_ */
