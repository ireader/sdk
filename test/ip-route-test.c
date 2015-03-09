#include "cstringext.h"
#include "ip-route.h"

static void ip_valid_test(void)
{
	// IPv4
	assert(1 == ip_valid("192.168.0.1"));
	assert(1 == ip_valid("255.255.255.255"));
	assert(1 == ip_valid("255.255.255.0"));
	assert(1 == ip_valid("0.255.255.255"));
	assert(1 == ip_valid("0.0.0.0"));
	assert(1 == ip_valid("127.0.0.1"));
	assert(0 == ip_valid("256.255.255.255"));
	assert(0 == ip_valid("abc.255.255.255"));
	assert(0 == ip_valid("www.abc.com"));
	assert(0 == ip_valid("abc.com"));
//	assert(0 == ip_valid("01.01.01.01"));
	assert(0 == ip_valid("0x1.0x1.0x1.0x1"));

	// IPv6
	// http://en.wikipedia.org/wiki/IPv6_address
	assert(1 == ip_valid("::"));	// Default unicast
	assert(1 == ip_valid("::1"));	// Localhost
	assert(1 == ip_valid("::ffff:0:0"));	// IPv4-mapped IPv6 address
	assert(1 == ip_valid("2002::"));	// 6to4
	assert(1 == ip_valid("ff02::a"));	// EIGRP Routers
	assert(1 == ip_valid("ff02::1:2"));	// All-dhcp-agents
	assert(1 == ip_valid("ff02::2:ff00:0")); // Node Information Queries
	assert(1 == ip_valid("2001:db8:85a3:8d3:1319:8a2e:370:7348"));
	assert(1 == ip_valid("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
	assert(1 == ip_valid("2001:db8:85a3:0:0:8a2e:370:7334"));
	assert(1 == ip_valid("2001:db8:85a3::8a2e:370:7334"));
	assert(0 == ip_valid("2001:db8:85a3:0:0:8a2e:370"));
}

void ip_route_test(void)
{
	char ip[40] = {0};

	ip_valid_test();
	ip_local(ip);
	printf("ip: %s\n", ip);

	ip_route_get("123.126.104.102", ip);
	printf("ip route(123.126.104.102) => %s\n", ip);
}
