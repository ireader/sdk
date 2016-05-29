#include "cstringext.h"
#include "sys/sock.h"

static void socket_ip_test(void)
{
	char ip[SOCKET_ADDRLEN] = {0};

	socket_ipv4(NULL, ip);

	// IPv4
	socket_ipv4("www.baidu.com", ip);
	socket_ipv4("abc.efg.ggg", ip);

	socket_ipv4("192.168.12.11", ip);
	socket_ipv4("0.0.0.0", ip);
	socket_ipv4("255.255.255.255", ip);
	socket_ipv4("343.123.1.1", ip);
	socket_ipv4("192.168.1", ip);

	// IPv6
	socket_ipv6("2001:0db8:0000:0000:0000:ff00:0042:8329", ip);
	socket_ipv6("2001:db8::ff00:42:8329", ip); // 2001:db8:0:0:0:ff00:42:8329
	socket_ipv6("::1", ip); // 0000:0000:0000:0000:0000:0000:0000:0001
}

static void socket_name_ipv4_test(void)
{
	unsigned short port;
	char ip[SOCKET_ADDRLEN];
	socket_t socket;
	socket = socket_connect_host("www.baidu.com", 80, -1);
	if (socket_invalid != socket)
	{
		socket_getname(socket, ip, &port);
		socket_getpeername(socket, ip, &port);
	}
	else
	{
		assert(0);
	}
	socket_close(socket);
}

static void socket_name_ipv6_test(void)
{
	unsigned short port;
	char ip[SOCKET_ADDRLEN];
	socket_t socket;
	socket = socket_connect_host("ipv6.baidu.com", 80, -1);
	if (socket_invalid != socket)
	{
		socket_getname(socket, ip, &port);
		socket_getpeername(socket, ip, &port);
	}
	else
	{
		assert(0);
	}
	socket_close(socket);
}

void socket_test(void)
{
	socket_init();

	socket_ip_test();
	socket_name_ipv4_test();
	socket_name_ipv6_test();

	socket_cleanup();
}
