#ifndef _tcpserver_h_
#define _tcpserver_h_

#include "sys/sock.h"

/// create a new TCP socket, bind, and listen
/// @param[in] ip socket bind local address, NULL-bind any address
/// @param[in] port bind local port
/// @param[in] backlog the maximum length to which the queue of pending connections for socket may grow
/// @return 0-error, use socket_geterror() to get error code, other-ok 
inline socket_t tcpserver_create(const char* ip, int port, int backlog)
{
	int ret;
	socket_t socket;
	struct sockaddr_in addr;

	// new a TCP socket
	socket = socket_tcp();
	if(socket_error == socket)
		return 0;

	// reuse addr
	socket_setreuseaddr(socket, 1);

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

	// listen
	if(0 == ret)
		ret = socket_listen(socket, backlog);

	if(0 != ret)
	{
		socket_close(socket);
		return 0;
	}

	return socket;
}

/// wait for client connection
/// @param[in] socket server socket(must be bound and listening)
/// @param[in/out] addr struct sockaddr_in for IPv4
/// @param[in/out] addrlen addr length in bytes
/// @param[in] mstimeout timeout in millisecond
/// @return 0-timeout, -1-error, use socket_geterror() to get error code, other-ok  
inline socket_t tcpserver_accept(socket_t socket, struct sockaddr* addr, socklen_t* addrlen, int mstimeout)
{
	int ret;
	socket_t client;

	ret = socket_select_read(socket, mstimeout);
	if(socket_error == ret)
	{
		return (socket_t)(-1);
	}
	else if(0 == ret)
	{
		return 0; // timeout
	}

	client = socket_accept(socket, addr, addrlen);
	if(socket_invalid == client)
		return (socket_t)(-1);

	return client;
}

#endif /* !_tcpserver_h_ */
