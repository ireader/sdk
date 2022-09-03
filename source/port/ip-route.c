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
#elif defined(OS_LINUX)
#include "route-netlink.h"
#endif

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#if defined(OS_WINDOWS)
static inline int socket_addr_mask_compare(uint8_t prefix, const struct sockaddr* addr1, const struct sockaddr* addr2)
{
	struct sockaddr_storage ss[2];
	return 0 == socket_addr_netmask(&ss[0], prefix, addr1) 
		&& 0 == socket_addr_netmask(&ss[1], prefix, addr2) 
		&& 0 == socket_addr_compare((const struct sockaddr*)&ss[0], (const struct sockaddr*)&ss[1]);
}

int ip_route_get(const char* destination, char ip[65])
{
	int r;
	DWORD index;
	DWORD dwRetVal;
	ULONG ulOutBufLen;
	struct addrinfo* ai;
	PIP_ADAPTER_UNICAST_ADDRESS_LH addr;
	PIP_ADAPTER_ADDRESSES pAdapter, pAdapterInfo;
		
	r = getaddrinfo(destination, NULL, NULL, &ai);
	if (0 != r)
		return r;
	
	if (NO_ERROR != GetBestInterfaceEx(ai->ai_addr, &index))
	{
		freeaddrinfo(ai);
		return -1;
	}

	ulOutBufLen = sizeof(IP_ADAPTER_ADDRESSES);
	pAdapterInfo = (PIP_ADAPTER_ADDRESSES)malloc(ulOutBufLen);
	dwRetVal = GetAdaptersAddresses(ai->ai_family, 0, NULL, pAdapterInfo, &ulOutBufLen);
	if (ERROR_BUFFER_OVERFLOW == dwRetVal)
	{
		free(pAdapterInfo);
		pAdapterInfo = (PIP_ADAPTER_ADDRESSES)malloc(ulOutBufLen);
		dwRetVal = GetAdaptersAddresses(ai->ai_family, 0, NULL, pAdapterInfo, &ulOutBufLen);
	}

	ip[0] = 0;
	if (ERROR_SUCCESS == dwRetVal)
	{
		for (pAdapter = pAdapterInfo; 0 == ip[0] && pAdapter; pAdapter = pAdapter->Next)
		{
			if (IfOperStatusUp != pAdapter->OperStatus || (pAdapter->IfIndex != index && pAdapter->Ipv6IfIndex != index))
				continue;
			//if (IF_TYPE_ETHERNET_CSMACD != pAdapter->IfType && IF_TYPE_IEEE80211 != pAdapter->IfType && IF_TYPE_SOFTWARE_LOOPBACK != pAdapter->IfType)
			//	continue;

			// todo: iter check netmask
			for (addr = pAdapter->FirstUnicastAddress; /*0 == ip[0] &&*/ addr; addr = addr->Next)
			{
				if (addr->Address.lpSockaddr->sa_family != ai->ai_family)
					continue;

				if (0 == ip[0] || (addr->OnLinkPrefixLength > 0 && socket_addr_mask_compare(addr->OnLinkPrefixLength, ai->ai_addr, addr->Address.lpSockaddr))) {

					if (AF_INET == addr->Address.lpSockaddr->sa_family)
					{
						inet_ntop(AF_INET, &((struct sockaddr_in*)addr->Address.lpSockaddr)->sin_addr, ip, 40);
					}
					else if (AF_INET6 == addr->Address.lpSockaddr->sa_family)
					{
						inet_ntop(AF_INET6, &((struct sockaddr_in6*)addr->Address.lpSockaddr)->sin6_addr, ip, 64);
					}
				}
			}
		}
	}

	freeaddrinfo(ai);
	free(pAdapterInfo);
	return dwRetVal == ERROR_SUCCESS ? (ip[0] ? 0 : -1) : -(int)dwRetVal;
}
#else

int ip_route_get(const char* destination, char ip[65])
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
    if(0 == r) {
        r = socket_addr_to((struct sockaddr*)&gateway, socket_addr_len((struct sockaddr*)&gateway), ip, &port);
	} else {
        r = ip_local(ip);
	}
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
			snprintf(ip, 40, "%d.%d.%d.%d", 
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
int ip_local(char ip[65])
{
    u_short port;
	struct ifaddrs *ifaddr, *ifa;

	if(0 != getifaddrs(&ifaddr))
		return -errno;

	for(ifa = ifaddr; ifa; ifa = ifa->ifa_next)
	{
		if(!ifa->ifa_addr || 0 == strncmp("lo", ifa->ifa_name, 2))
			continue;

        if(AF_INET != ifa->ifa_addr->sa_family || AF_INET6 != ifa->ifa_addr->sa_family)
            continue;
        
        socket_addr_to(ifa->ifa_addr, socket_addr_len(ifa->ifa_addr), ip, &port);
        break;
	}

	freeifaddrs(ifaddr);
	return 0;
}
#endif

#endif
