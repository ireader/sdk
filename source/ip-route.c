#include "ip-route.h"
#if defined(OS_WINDOWS)
#include <winsock2.h>
#include <iphlpapi.h>
#include <WS2tcpip.h>
#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

#else
#include <sys/types.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#if defined(OS_WINDOWS)
static int ipv4_valid(const char* ip)
{
	int a, b, c, d;
	if(ip && 4 == sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d))
	{
		if(0 <= a && a <= 255 && 0 <= b && b <= 255 && 0 <= c && c <= 255 && 0 <= d && d <= 255)
			return 1;
	}
	return 0;
}

static int ipv6_valid(const char* src)
{
#if 0
	unsigned char addr[sizeof(struct in6_addr)];
	return 1 == InetPton(AF_INET6, src, addr) ? 1 : 0;
#else
	// freebsd
	// lib\libc\inet\inet_pton.c
	static const char xdigits_l[] = "0123456789abcdef",
			  xdigits_u[] = "0123456789ABCDEF";
	unsigned char tmp[16], *tp, *endp, *colonp;
	const char *xdigits, *curtok;
	int ch, seen_xdigits;
	unsigned int val;

	memset((tp = tmp), '\0', sizeof(tmp));
	endp = tp + sizeof(tmp);
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if (*src == ':')
		if (*++src != ':')
			return (0);
	curtok = src;
	seen_xdigits = 0;
	val = 0;
	while ((ch = *src++) != '\0') {
		const char *pch;

		if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
			pch = strchr((xdigits = xdigits_u), ch);
		if (pch != NULL) {
			val <<= 4;
			val |= (pch - xdigits);
			if (++seen_xdigits > 4)
				return (0);
			continue;
		}
		if (ch == ':') {
			curtok = src;
			if (!seen_xdigits) {
				if (colonp)
					return (0);
				colonp = tp;
				continue;
			} else if (*src == '\0') {
				return (0);
			}
			if (tp + 2 > endp)
				return (0);
			*tp++ = (u_char) (val >> 8) & 0xff;
			*tp++ = (u_char) val & 0xff;
			seen_xdigits = 0;
			val = 0;
			continue;
		}
		//if (ch == '.' && ((tp + NS_INADDRSZ) <= endp) &&
		//    inet_pton4(curtok, tp) > 0) {
		//	tp += NS_INADDRSZ;
		//	seen_xdigits = 0;
		//	break;	/*%< '\\0' was seen by inet_pton4(). */
		//}
		return (0);
	}
	if (seen_xdigits) {
		if (tp + 2 > endp)
			return (0);
		*tp++ = (u_char) (val >> 8) & 0xff;
		*tp++ = (u_char) val & 0xff;
	}
	if (colonp != NULL) {
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		if (tp == endp)
			return (0);
		for (i = 1; i <= n; i++) {
			endp[- i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp)
		return (0);
	return (1);
#endif
}
#else
static int ipv4_valid(const char* ip)
{
	unsigned char addr[sizeof(struct in_addr)];
	return 1 == inet_pton(AF_INET, ip, addr) ? 1 : 0;
}

static int ipv6_valid(const char* ip)
{
	unsigned char addr[sizeof(struct in6_addr)];
	return 1 == inet_pton(AF_INET6, ip, addr) ? 1 : 0;
}
#endif

int ip_valid(const char* ip)
{
	return (1==ipv4_valid(ip) || 1==ipv6_valid(ip)) ? 1 : 0;
}

#if defined(OS_WINDOWS)
int ip_route_get(const char* distination, char ip[40])
{
	DWORD index = ~(-1);
	struct sockaddr_in addrin;
	MIB_IPADDRTABLE *table = NULL;
	ULONG dwSize = 0;
	DWORD errcode = 0;
	DWORD i = 0;

	addrin.sin_family = AF_INET;
	addrin.sin_port = htons(0);
	inet_pton(AF_INET, distination, &addrin.sin_addr);
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
static uint32_t ipv4_iface_addr(const char* iface)
{
	uint32_t addr;
	struct ifaddrs *ifaddr, *ifa;

	if(0 != getifaddrs(&ifaddr))
		return INADDR_ANY;

	addr = INADDR_ANY;
	for(ifa = ifaddr; ifa; ifa = ifa->ifa_next)
	{
		if(!ifa->ifa_addr || AF_INET != ifa->ifa_addr->sa_family || 0 != strcmp(iface, ifa->ifa_name))
			continue;

		addr = ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr.s_addr;
		break;
	}

	freeifaddrs(ifaddr);
	return addr;
}

/*
[root@localhost net]# cat route
Iface	Destination	Gateway 	Flags	RefCnt	Use	Metric	Mask		MTU	Window	IRTT                                                       
eth0	000CA8C0	00000000	0001	0		0	0		00FFFFFF	0	0	0                                                                               
eth0	0000FEA9	00000000	0001	0		0	0		0000FFFF	0	0	0                                                                               
eth0	00000000	010CA8C0	0003	0		0	0		00000000	0	0	0      
*/
static uint32_t ipv4_route(uint32_t peer)
{
	FILE* fp;
	char iface[18], iface0[18], line[1024];
	uint32_t destination, gateway, netmask;

	fp = fopen("/proc/net/route", "r");
	if(!fp)
		return INADDR_ANY;

	iface0[0] = '\0';
	fgets(line, sizeof(line), fp); // filter first line

	while(NULL != fgets(line, sizeof(line), fp))
	{
		if(4 == sscanf(line, "%16s %X %X %*X %*d %*d %*d %X", iface, &destination, &gateway, &netmask))
		{
			assert((destination & netmask) == destination);
			if(0 == destination)
			{
				strcpy(iface0, iface); // save default gateway
			}
			else if((peer & netmask) == destination)
			{
				strcpy(iface0, iface); // found
				break;
			}
		}
	}

	fclose(fp);

	return ipv4_iface_addr(iface0);
}

int ip_route_get(const char* distination, char ip[40])
{
#if 1
	struct in_addr addr;
	inet_pton(AF_INET, distination, &addr);
	addr.s_addr = ipv4_route(addr.s_addr);
	return NULL==inet_ntop(AF_INET, &addr, ip, INET6_ADDRSTRLEN) ? errno : 0;
#else
	size_t n = 0;
	FILE *fp = NULL;
	char cmd[128] = {0};

	// "ip route get 255.255.255.255 | grep -Po '(?<=src )(\d{1,3}.){4}'"
	snprintf(cmd, sizeof(cmd), "ip route get %s | grep -Po '(?<=src )(\\d{1,3}.){4}'", distination);
	fp = popen(cmd, "r");
	if(!fp)
		return -1;

	fgets(cmd, sizeof(cmd)-1, fp);
	pclose(fp);

	n = strlen(cmd);
	while(n > 0 && strchr(" \r\n\t", cmd[n-1]))
	{
		cmd[--n] = '\0';
	}

	if(n < 40 && ip_valid(cmd))
	{
		strncpy(ip, cmd, n);
		ip[n] = '\0';
		return 0;
	}
	return -1;
#endif
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
int ip_local(char ip[40])
{
	struct ifaddrs *ifaddr, *ifa;

	if(0 != getifaddrs(&ifaddr))
		return errno;

	for(ifa = ifaddr; ifa; ifa = ifa->ifa_next)
	{
		if(!ifa->ifa_addr || AF_INET != ifa->ifa_addr->sa_family || 0 == strcmp("lo", ifa->ifa_name))
			continue;

		inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, ip, 40);
		break;
	}

	freeifaddrs(ifaddr);
	return 0;
}
#endif
