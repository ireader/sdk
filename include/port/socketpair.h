#ifndef _socketpair_h_
#define _socketpair_h_

#if !defined(OS_WINDOWS)
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>

// https://linux.die.net/man/3/socketpair
#else

#include "sockutil.h"
#include <stdlib.h>
#include <string.h>

/// @param[in] domain AF_UNIX/AF_LOCAL only
/// @param[in] type SOCK_STREAM/SOCK_DGRAM/SOCK_SEQPACKET only
/// @param[in] protocol ignore, must be 0
/// @return 0-ok, -1-error
static inline int socketpair(int domain, int type, int protocol, socket_t sv[2])
{
	socket_t pr[2];
	socklen_t addrlen;
	struct addrinfo hints, *ai;
	struct sockaddr_storage addr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = domain;
	hints.ai_socktype = type;
	hints.ai_flags = AI_PASSIVE;
	if (0 != getaddrinfo("", NULL, &hints, &ai))
		return -1;

	addrlen = ai->ai_addrlen;
	memcpy(&addr, ai->ai_addr, ai->ai_addrlen);
	freeaddrinfo(ai);

	pr[0] = socket(domain, type, protocol);
	pr[1] = socket(domain, type, protocol);
	if (socket_invalid == pr[0] || socket_invalid == pr[1])
		goto SOCKPAIRFAILED;

	if(0 != socket_bind(pr[0], (struct sockaddr*)&addr, addrlen))
		goto SOCKPAIRFAILED;

	if (SOCK_STREAM == type && 0 != socket_listen(pr[0], 1))
		goto SOCKPAIRFAILED;

	addrlen = sizeof(addr);
	if (socket_error == getsockname(pr[0], (struct sockaddr*)&addr, &addrlen))
		goto SOCKPAIRFAILED;

	if(0 != socket_connect_by_time(pr[1], (struct sockaddr*)&addr, addrlen, 10))
		goto SOCKPAIRFAILED;

	if (SOCK_STREAM == type)
	{
		addrlen = sizeof(addr);
		sv[0] = socket_accept_by_time(pr[0], &addr, &addrlen, 10);
		if (socket_invalid == sv[0])
			goto SOCKPAIRFAILED;

		socket_close(pr[0]); // close daemon socket
	}
	else
	{
		addrlen = sizeof(addr);
		if (socket_error == getsockname(pr[1], (struct sockaddr*)&addr, &addrlen))
			goto SOCKPAIRFAILED;

		if (0 != socket_connect_by_time(pr[0], (struct sockaddr*)&addr, addrlen, 10))
			goto SOCKPAIRFAILED;

		sv[0] = pr[0];
	}

	socket_setnonblock(sv[0], 1);
	sv[1] = pr[1];
	return 0;

SOCKPAIRFAILED:
	if (socket_invalid != pr[0])
		socket_close(pr[0]);
	if (socket_invalid != pr[1])
		socket_close(pr[1]);
	return -1;
}
#endif

#endif /* !_socketpair_h_ */
