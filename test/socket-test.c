#include "cstringext.h"
#include "sockutil.h"

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
	socket_ipv6("::ffff:c000:0280", ip); // ::ffff:192.0.2.128
	socket_ipv6("::ffff:192.0.2.128", ip); // ::ffff:c000:0280 IPv4-mapped IPv6 address
}

static void socket_name_ipv4_test(void)
{
    size_t size;
	unsigned short port;
	char ip[SOCKET_ADDRLEN];
	socket_t socket;
	socket = socket_connect_host("www.baidu.com", 80, -1);
	if (socket_invalid != socket)
	{
		socket_getname(socket, ip, &port);
		socket_getpeername(socket, ip, &port);
        socket_getrecvbuf(socket, &size);
        socket_getsendbuf(socket, &size);
        socket_getrecvtimeout(socket, &size);
        socket_getsendtimeout(socket, &size);
	}
	else
	{
		assert(0);
	}
	socket_close(socket);
}

static void socket_name_ipv6_test(void)
{
    size_t size;
	unsigned short port;
	char ip[SOCKET_ADDRLEN];
	socket_t socket;
	socket = socket_connect_host("ipv6.baidu.com", 80, -1);
	if (socket_invalid != socket)
	{
		socket_getname(socket, ip, &port);
		socket_getpeername(socket, ip, &port);
        socket_getrecvbuf(socket, &size);
        socket_getsendbuf(socket, &size);
        socket_getrecvtimeout(socket, &size);
        socket_getsendtimeout(socket, &size);
	}
	else
	{
		assert(0);
	}
	socket_close(socket);
}

static void socket_addr_is_multicast_test(void)
{
	struct sockaddr_in in;
	struct sockaddr_in6 in6;

	socket_addr_from_ipv4(&in, "224.0.0.0", 0);
	assert(socket_addr_is_multicast((struct sockaddr*)&in, sizeof(in)));
	socket_addr_from_ipv4(&in, "224.255.255.255", 0);
	assert(socket_addr_is_multicast((struct sockaddr*)&in, sizeof(in)));
	socket_addr_from_ipv4(&in, "239.0.0.0", 0);
	assert(socket_addr_is_multicast((struct sockaddr*)&in, sizeof(in)));
	socket_addr_from_ipv4(&in, "239.255.255.255", 0);
	assert(socket_addr_is_multicast((struct sockaddr*)&in, sizeof(in)));

	socket_addr_from_ipv4(&in, "240.0.0.0", 0);
	assert(0 == socket_addr_is_multicast((struct sockaddr*)&in, sizeof(in)));
	socket_addr_from_ipv4(&in, "223.255.255.255", 0);
	assert(0 == socket_addr_is_multicast((struct sockaddr*)&in, sizeof(in)));

	socket_addr_from_ipv6(&in6, "ff00::", 0);
	assert(socket_addr_is_multicast((struct sockaddr*)&in6, sizeof(in6)));
	socket_addr_from_ipv6(&in6, "ffff::", 0);
	assert(socket_addr_is_multicast((struct sockaddr*)&in6, sizeof(in6)));

	socket_addr_from_ipv6(&in6, "::", 0); // 0:0:0:0:0:0:0:0
	assert(0 == socket_addr_is_multicast((struct sockaddr*)&in6, sizeof(in6)));
	socket_addr_from_ipv6(&in6, "::1", 0); // 0:0:0:0:0:0:0:1
	assert(0 == socket_addr_is_multicast((struct sockaddr*)&in6, sizeof(in6)));
}

void socket_opt_dontfrag_test(void);

//static void socket_accept_errno_9_test()
//{
//	int i, n;
//	char msg[128];
//	socket_t s, c;
//	struct sockaddr_storage addr;
//	socklen_t addrlen;
//
//	s = socket_tcp_listen(AF_INET, NULL, 33333, SOMAXCONN, 0, 0);
//	assert(socket_invalid != s);
//
//	c = socket_connect_host("127.0.0.1", 33333, 5000);
//	socket_close(c);
//
//	addrlen = sizeof(addr);
//	c = socket_accept(s, &addr, &addrlen);
//	printf("errno: %d\n", errno);
//	assert(socket_invalid != c);
//
//	n = socket_recv_by_time(c, msg, sizeof(msg), 0, 1000);
//	socket_send_all_by_time(c, msg, n, 0, 1000); // echo
//	socket_close(c);
//	socket_close(s);
//}

void socket_test(void)
{
	socket_init();

	//socket_accept_errno_9_test();
	socket_ip_test();
	socket_name_ipv4_test();
	socket_name_ipv6_test();
	socket_addr_is_multicast_test();
    
#if !defined(OS_MAC)
    socket_opt_dontfrag_test();
#endif
    
	socket_cleanup();
}
