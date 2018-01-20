#include "udp-socket.h"
#include "sockutil.h"

int udp_socket_create(u_short port, struct udp_socket_t* s)
{
	s->port = port;

	// https://msdn.microsoft.com/en-us/library/windows/desktop/bb513665(v=vs.85).aspx
	// IP Addresses with a Dual-Stack Socket
	s->udp = socket(AF_INET, SOCK_DGRAM, 0);
	s->udp6 = socket(AF_INET6, SOCK_DGRAM, 0);

	if (socket_invalid != s->udp)
	{
		if (0 != socket_bind_any(s->udp, port))
		{
			socket_close(s->udp);
			s->udp = socket_invalid;
		}
	}

	if (socket_invalid != s->udp6)
	{
		socket_setipv6only(s->udp6, 1); // disable Dual-Stack Socket
		if (0 != socket_bind_any(s->udp6, port))
		{
			socket_close(s->udp6);
			s->udp6 = socket_invalid;
		}
	}

	if (socket_invalid == s->udp && socket_invalid == s->udp6)
	{
		udp_socket_destroy(s);
		return -1;
	}

	return 0;
}

int udp_socket_destroy(struct udp_socket_t* socket)
{
	int i;
	socket_t udp[2];
	udp[0] = socket->udp;
	udp[1] = socket->udp6;

	for (i = 0; i < sizeof(udp) / sizeof(udp[0]); i++)
	{
		if (socket_invalid != udp[i])
		{
			socket_close(udp[i]);
			udp[i] = socket_invalid;
		}
	}

	return 0;
}

int udp_socket_sendto(const struct udp_socket_t* s, const void* buf, size_t len, const struct sockaddr_storage* addr)
{
	if (AF_INET == addr->ss_family && socket_invalid != s->udp)
	{
		return socket_sendto(s->udp, buf, len, 0, (const struct sockaddr*)addr, sizeof(struct sockaddr_in));
	}
	else if (AF_INET6 == addr->ss_family && socket_invalid != s->udp6)
	{
		return socket_sendto(s->udp6, buf, len, 0, (const struct sockaddr*)addr, sizeof(struct sockaddr_in6));
	}
	else
	{
		assert(0);
		return -1;
	}
}
