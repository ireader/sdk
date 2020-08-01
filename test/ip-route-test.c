#include "sys/sock.h"
#include "port/ip-route.h"
#include <stdio.h>
#include <assert.h>

static void ip_valid_test(void)
{
	// IPv4
	assert(1 == socket_isip("192.168.0.1"));
	assert(1 == socket_isip("255.255.255.255"));
	assert(1 == socket_isip("255.255.255.0"));
	assert(1 == socket_isip("0.255.255.255"));
	assert(1 == socket_isip("0.0.0.0"));
	assert(1 == socket_isip("127.0.0.1"));
	assert(0 == socket_isip("256.255.255.255"));
	assert(0 == socket_isip("abc.255.255.255"));
	assert(0 == socket_isip("www.abc.com"));
	assert(0 == socket_isip("abc.com"));
//	assert(0 == socket_isip("01.01.01.01"));
	assert(0 == socket_isip("0x1.0x1.0x1.0x1"));

	// IPv6
	// http://en.wikipedia.org/wiki/IPv6_address
	assert(1 == socket_isip("::"));	// Default unicast
	assert(1 == socket_isip("::1"));	// Localhost
	assert(1 == socket_isip("::ffff:0:0"));	// IPv4-mapped IPv6 address
	assert(1 == socket_isip("2002::"));	// 6to4
	assert(1 == socket_isip("ff02::a"));	// EIGRP Routers
	assert(1 == socket_isip("ff02::1:2"));	// All-dhcp-agents
	assert(1 == socket_isip("ff02::2:ff00:0")); // Node Information Queries
	assert(1 == socket_isip("2001:db8:85a3:8d3:1319:8a2e:370:7348"));
	assert(1 == socket_isip("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
	assert(1 == socket_isip("2001:db8:85a3:0:0:8a2e:370:7334"));
	assert(1 == socket_isip("2001:db8:85a3::8a2e:370:7334"));
	assert(0 == socket_isip("2001:db8:85a3:0:0:8a2e:370"));
}

void ip_route_test(void)
{
	int i;
	char ip[40] = {0};
	const char* remotes[] = { "192.168.12.114", "127.0.0.1", "::1", "::ffff:127.0.0.1" };

	ip_valid_test();
	ip_local(ip);
	printf("ip: %s\n", ip);

	for (i = 0; i < sizeof(remotes) / sizeof(remotes[0]); i++)
	{
		memset(ip, 0, sizeof(ip));
		ip_route_get(remotes[i], ip);
		printf("ip route(%s) => %s\n", remotes[i], ip);
	}

	// don't support DNS
	//ip_route_get("www.baidu.com", ip);
	//printf("ip route(www.baidu.com) => %s\n", ip);
}
