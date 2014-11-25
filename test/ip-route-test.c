#include "cstringext.h"
#include "ip-route.h"

void ip_route_test(void)
{
	char ip[40] = {0};
	ip_route_get("123.126.104.102", ip);
}
