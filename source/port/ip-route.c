#include "port/ip-route.h"
#include "sys/sock.h"

#if defined(OS_WINDOWS)
#include <winsock2.h>
#include <iphlpapi.h>
#include <WS2tcpip.h>
#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#if !defined(__ANDROID_API__) || __ANDROID_API__ >= 24
#include <ifaddrs.h>
#endif

#if defined(OS_MAC)
#include "route-rtm.h"
#elif defined(OS_ANDROID)
#include "route-linux.h"
#else
#include "route-netlink.h"
#endif

#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#if defined(OS_WINDOWS)
int ip_route_get(const char* destination, char ip[40])
{
	DWORD index = ~(-1);
	struct sockaddr_in addrin;
	MIB_IPADDRTABLE *table = NULL;
	ULONG dwSize = 0;
	DWORD errcode = 0;
	DWORD i = 0;

	addrin.sin_family = AF_INET;
	addrin.sin_port = htons(0);
	inet_pton(AF_INET, destination, &addrin.sin_addr);
	if(NO_ERROR != GetBestInterfaceEx((struct sockaddr*)&addrin, &index))
		return -1;

	errcode = GetIpAddrTable( table, &dwSize, 0 );
	assert(ERROR_INSUFFICIENT_BUFFER == errcode);

	table = (MIB_IPADDRTABLE*)malloc(dwSize);
	errcode = GetIpAddrTable( table, &dwSize, 0 );
	if(!table || NO_ERROR != errcode)
	{
		free(table);
		return -1;
	}

	ip[0] = '\0';
	for(i = 0; i < table->dwNumEntries; i++)
	{
		if(table->table[i].dwIndex == index)
		{
			sprintf(ip, "%d.%d.%d.%d", 
				(table->table[i].dwAddr >> 0) & 0xFF,
				(table->table[i].dwAddr >> 8) & 0xFF,
				(table->table[i].dwAddr >> 16) & 0xFF,
				(table->table[i].dwAddr >> 24) & 0xFF);
			break;
		}
	}

	free(table);
	return 0==ip[0] ? -1 : 0;
}
#else

int ip_route_get(const char* destination, char ip[40])
{
    int r;
    u_short port;
    socklen_t dstlen;
    struct sockaddr_storage dst;
    struct sockaddr_storage gateway;
    memset(&gateway, 0, sizeof(gateway));
    r = socket_addr_from(&dst, &dstlen, destination, 0);
    if(0 != r)
        return r;
    
    r = router_gateway((struct sockaddr*)&dst, &gateway);
    if(0 == r)
        r = socket_addr_to((struct sockaddr*)&gateway, socket_addr_len((struct sockaddr*)&gateway), ip, &port);
	return r;
}
#endif

#if defined(OS_WINDOWS)
int ip_local(char ip[40])
{
	MIB_IPADDRTABLE *table = NULL;
	ULONG dwSize = 0;
	DWORD errcode = 0;
	DWORD i = 0;

	errcode = GetIpAddrTable( table, &dwSize, 0 );
	assert(ERROR_INSUFFICIENT_BUFFER == errcode);

	table = (MIB_IPADDRTABLE*)malloc(dwSize);
	errcode = GetIpAddrTable( table, &dwSize, 0 );
	if(!table || NO_ERROR != errcode)
	{
		free(table);
		return -1;
	}

	ip[0] = '\0';
	for(i = 0; i < table->dwNumEntries; i++)
	{
		if(table->table[i].wType & MIB_IPADDR_PRIMARY)
		{
			sprintf(ip, "%d.%d.%d.%d", 
				(table->table[i].dwAddr >> 0) & 0xFF,
				(table->table[i].dwAddr >> 8) & 0xFF,
				(table->table[i].dwAddr >> 16) & 0xFF,
				(table->table[i].dwAddr >> 24) & 0xFF);
			break;
		}
	}

	free(table);
	return 0==ip[0] ? -1 : 0;
}
#else

#if !defined(__ANDROID_API__) || __ANDROID_API__ >= 24
int ip_local(char ip[40])
{
	struct ifaddrs *ifaddr, *ifa;

	if(0 != getifaddrs(&ifaddr))
		return errno;

	for(ifa = ifaddr; ifa; ifa = ifa->ifa_next)
	{
		if(!ifa->ifa_addr || AF_INET != ifa->ifa_addr->sa_family || 0 == strncmp("lo", ifa->ifa_name, 2))
			continue;

		inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, ip, 40);
		break;
	}

	freeifaddrs(ifaddr);
	return 0;
}
#endif

#endif
