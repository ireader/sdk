#ifndef _udpsocket_h_
#define _udpsocket_h_

#include "sys/sock.h"

inline socket_t udpsocket_create(const char* ip, int port)
{
	int ret;
	socket_t socket;
	struct sockaddr_in addr;

	// new a UDP socket
	socket = socket_udp();
	if(socket_error == socket)
		return socket_invalid;

	// reuse addr
//	socket_setreuseaddr(socket, 1);

	// bind
	if(ip && ip[0])
	{
		ret = socket_addr_ipv4(&addr, ip, (unsigned short)port);
		if(0 == ret)
			ret = socket_bind(socket, (struct sockaddr*)&addr, sizeof(addr));
	}
	else
	{
		ret = socket_bind_any(socket, (unsigned short)port);
	}

	if(0 != ret)
	{
		socket_close(socket);
		return socket_invalid;
	}

	return socket;
}

#endif /* !_udpsocket_h_ */
