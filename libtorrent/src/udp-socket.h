#ifndef _udp_socket_h_
#define _udp_socket_h_

#include "sys/sock.h"

struct udp_socket_t
{
	u_short port;
	socket_t udp;
	socket_t udp6;
};

int udp_socket_create(u_short port, struct udp_socket_t* socket);
int udp_socket_destroy(struct udp_socket_t* socket);

int udp_socket_sendto(const struct udp_socket_t* socket, const void* buf, size_t len, const struct sockaddr_storage* addr);

#endif /* !_udp_socket_h_ */
