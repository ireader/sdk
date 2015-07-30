#include "cstringext.h"
#include "sys/sock.h"

static void socket_ip_test(void)
{
	char ip[40] = {0};

	// IPv4
	socket_ip("www.baidu.com", ip);
	socket_ip("abc.efg.gg", ip);

	socket_ip("192.168.12.11", ip);
	socket_ip("0.0.0.0", ip);
	socket_ip("255.255.255.255", ip);
	socket_ip("343.123.1.1", ip);
	socket_ip("192.168.1", ip);

	// IPv6
	socket_ip("2001:0db8:0000:0000:0000:ff00:0042:8329", ip);
	socket_ip("2001:db8::ff00:42:8329", ip); // 2001:db8:0:0:0:ff00:42:8329
	socket_ip("::1", ip); // 0000:0000:0000:0000:0000:0000:0000:0001
}

void socket_test(void)
{
	socket_init();

	socket_ip_test();

	socket_cleanup();
}
