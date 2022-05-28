#include "port/network.h"

#if defined(_WIN32) || defined(_WIN64)
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <Winsock2.h>
#include <WS2tcpip.h>
#include <IPHlpApi.h>
#include <Windows.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Mpr.lib")

#ifndef IF_TYPE_IEEE80211
#define IF_TYPE_IEEE80211 71
#endif

#else
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#if !defined(OS_MAC)
#include <netpacket/packet.h>
#endif
#include <net/if.h>
#include <ifaddrs.h>
#include <netdb.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../deprecated/tools.h"


#if defined(_WIN32) || defined(_WIN64)
//int network_netadaptors_count()
//{
//	// MSDN:
//	// The GetNumberOfInterfaces function returns the number of interfaces on the local computer, including the loopback interface. 
//	// This number is one more than the number of adapters returned by the GetAdaptersInfo and GetInterfaceInfo functions 
//	// because these functions do not return information about the loopback interface.
//
//	DWORD num = 0;
//	if(NO_ERROR == GetNumberOfInterfaces(&num))
//		return num>0?num-1:0;
//	return -1;
//}

#if 0
int network_getip(network_getip_fcb fcb, void* param)
{
	UINT i = 0;
	ULONG ulOutBufLen;
	DWORD dwRetVal = 0;
	IP_ADDR_STRING *addr;
	PIP_ADAPTER_INFO pAdapter, pAdapterInfo;
	char hwaddr[MAX_ADAPTER_ADDRESS_LENGTH*3] = {0};

	// Make an initial call to GetAdaptersInfo to get
	// the necessary size into the ulOutBufLen variable
	ulOutBufLen = sizeof(IP_ADAPTER_INFO);
	pAdapterInfo = (PIP_ADAPTER_INFO)malloc(ulOutBufLen);
	if (GetAdaptersInfo( pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) 
	{
		free(pAdapterInfo);
		pAdapterInfo = (PIP_ADAPTER_INFO) malloc (ulOutBufLen);
	}

	if ((dwRetVal = GetAdaptersInfo( pAdapterInfo, &ulOutBufLen)) == ERROR_SUCCESS)
	{
		for(pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next)
		{
			if(IF_TYPE_ETHERNET_CSMACD!=pAdapter->Type && IF_TYPE_IEEE80211!=pAdapter->Type)
				continue;

			// mac address
			for(i=0; i<pAdapter->AddressLength; i++)
			{
				if(i > 0) hwaddr[i*3-1] = ':';
				sprintf(hwaddr+i*3, "%02X", (int)pAdapter->Address[i]);
			}

			for (addr = &pAdapter->IpAddressList; addr; addr = addr->Next)
			{
				fcb(param,
					hwaddr,										// mac address
					pAdapter->AdapterName,						// name
					0 == pAdapter->DhcpEnabled ? 0 : 1,			// dhcp
					addr->IpAddress.String,						// ip address
					addr->IpMask.String,						// netmask
					pAdapter->GatewayList.IpAddress.String);	// gateway
			}
		}
	}

	free(pAdapterInfo);
	return dwRetVal==ERROR_SUCCESS?0:-(int)dwRetVal;
}
#else
int network_getip(network_getip_fcb fcb, void* param)
{
	UINT i = 0;
	ULONG ulOutBufLen;
	DWORD dwRetVal = 0;
	IN_ADDR netmask;
	PIP_ADAPTER_UNICAST_ADDRESS_LH addr;
	PIP_ADAPTER_ADDRESSES pAdapter, pAdapterInfo;
	char hwaddr[MAX_ADAPTER_ADDRESS_LENGTH * 3] = { 0 };
	char ip[65] = { 0 };
	char subnet[65] = { 0 };
	char gateway[65] = { 0 };
	char description[128] = { 0 };

	// Make an initial call to GetAdaptersInfo to get
	// the necessary size into the ulOutBufLen variable
	ulOutBufLen = sizeof(IP_ADAPTER_ADDRESSES);
	pAdapterInfo = (PIP_ADAPTER_ADDRESSES)malloc(ulOutBufLen);
	if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, NULL, pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)
	{
		free(pAdapterInfo);
		pAdapterInfo = (PIP_ADAPTER_ADDRESSES)malloc(ulOutBufLen);
	}

	if ((dwRetVal = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, NULL, pAdapterInfo, &ulOutBufLen)) == ERROR_SUCCESS)
	{
		for (pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next)
		{
			if (IfOperStatusUp != pAdapter->OperStatus)
				continue;
			if (IF_TYPE_ETHERNET_CSMACD != pAdapter->IfType && IF_TYPE_IEEE80211 != pAdapter->IfType)
				continue;

			memset(description, 0, sizeof(description));
			WideCharToMultiByte(CP_UTF8, 0, pAdapter->Description, -1, description, sizeof(description) - 1, NULL, NULL);

			// mac address
			for (i = 0; i < pAdapter->PhysicalAddressLength; i++)
			{
				if (i > 0) hwaddr[i * 3 - 1] = ':';
				sprintf(hwaddr + i * 3, "%02X", (int)pAdapter->PhysicalAddress[i]);
			}

			if (pAdapter->FirstGatewayAddress && AF_INET == pAdapter->FirstGatewayAddress->Address.lpSockaddr->sa_family)
				inet_ntop(AF_INET, &((struct sockaddr_in*)pAdapter->FirstGatewayAddress->Address.lpSockaddr)->sin_addr, gateway, sizeof(gateway));
			else if (pAdapter->FirstGatewayAddress && AF_INET6 == AF_INET == pAdapter->FirstGatewayAddress->Address.lpSockaddr->sa_family)
				inet_ntop(AF_INET6, &((struct sockaddr_in6*)pAdapter->FirstGatewayAddress->Address.lpSockaddr)->sin6_addr, gateway, sizeof(gateway));
			else
				memset(gateway, 0, sizeof(gateway));

			for (addr = pAdapter->FirstUnicastAddress; addr; addr = addr->Next)
			{
				if (AF_INET == addr->Address.lpSockaddr->sa_family)
				{
					ConvertLengthToIpv4Mask(addr->OnLinkPrefixLength, &netmask.s_addr);
					inet_ntop(AF_INET, &netmask, subnet, sizeof(subnet));
					inet_ntop(AF_INET, &((struct sockaddr_in*)addr->Address.lpSockaddr)->sin_addr, ip, sizeof(ip));
				}
				else if (AF_INET6 == addr->Address.lpSockaddr->sa_family)
				{
					memset(subnet, 0, sizeof(subnet));
					inet_ntop(AF_INET6, &((struct sockaddr_in6*)addr->Address.lpSockaddr)->sin6_addr, ip, sizeof(ip));
				}
				else
				{
					continue;
				}

				fcb(param, hwaddr, description[0] ? description : pAdapter->AdapterName, pAdapter->Dhcpv4Enabled, ip, subnet, gateway);
			}
		}
	}

	free(pAdapterInfo);
	return dwRetVal == ERROR_SUCCESS ? 0 : -(int)dwRetVal;
}
#endif

typedef int (CALLBACK* DNSFLUSHPROC)();
typedef int (CALLBACK* DHCPNOTIFYPROC)(LPWSTR, LPWSTR, BOOL, DWORD, DWORD, DWORD, int);
static int network_notify_ipchanged(const char* name, const char* ip, const char* netmask, int dhcp)
{
	// Nofity ip change

	DWORD dwIp;
	DWORD dwNetMask;
	HINSTANCE handle;
	DHCPNOTIFYPROC pDhcpNotifyProc;
	WCHAR wAdapterName[256] = {0};
	
	handle = LoadLibraryA("dhcpcsvc");
	if(!handle)
		return -1;

	pDhcpNotifyProc = (DHCPNOTIFYPROC)GetProcAddress(handle, "DhcpNotifyConfigChange");
	if(NULL != pDhcpNotifyProc)
	{
		dwIp = inet_addr(ip);
		dwNetMask = inet_addr(netmask);
		
		MultiByteToWideChar(CP_ACP, 0, name, -1, wAdapterName, 256);
		pDhcpNotifyProc(NULL, wAdapterName, TRUE, 0, dwIp, dwNetMask, dhcp?1:2);
	}
	FreeLibrary(handle);
	return 0;
}

static int network_notify_dnsflush()
{
	DNSFLUSHPROC pDnsFlushProc;
	HINSTANCE handle = LoadLibraryA("dnsapi");
	if(!handle)
		return -1;

	pDnsFlushProc = (DNSFLUSHPROC)GetProcAddress(handle, "DnsFlushResolverCache");
	if(NULL != pDnsFlushProc)
		pDnsFlushProc();

	FreeLibrary(handle);
	return 0;
}

#define VALIDATE_IPADDR(ip) (ip && *ip && inet_addr(ip))

int network_setip(const char* name, int enableDHCP, const char* ipaddr, const char* netmask, const char* gateway)
{
	HKEY hKey;
	char key[MAX_PATH];
	char mszIp[100] = {0};
	char mszNetmask[100] = {0};
	char mszGateway[100] = {0};
	DWORD dhcp = enableDHCP ? 1 : 0;

	strcpy(key, "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\");
	strcat(key, name);
	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
		return -1;

	// REG_MULTI_SZ数据需要在后面再加个0
	if(VALIDATE_IPADDR(ipaddr)) strcpy(mszIp, ipaddr);
	if(VALIDATE_IPADDR(netmask)) strcpy(mszNetmask, netmask);
	if(VALIDATE_IPADDR(gateway)) strcpy(mszGateway, gateway);

	RegSetValueExA(hKey, "IPAddress", 0, REG_MULTI_SZ, (BYTE*)(CONST char*)mszIp, strlen(mszIp)+2);
	RegSetValueExA(hKey, "SubnetMask", 0, REG_MULTI_SZ, (BYTE*)(CONST char*)mszNetmask, strlen(mszNetmask)+2);
	RegSetValueExA(hKey, "DefaultGateway", 0, REG_MULTI_SZ, (BYTE*)(CONST char*)mszGateway, strlen(mszGateway)+2);
	RegSetValueExA(hKey, "EnableDHCP", 0, REG_DWORD, (BYTE*)&dhcp, sizeof(dhcp));

	RegCloseKey(hKey);

	// Nofity ip change
	network_notify_ipchanged(name, ipaddr, netmask, enableDHCP);
	return 0;
}

int network_getdns(const char* name, char primary[65], char secondary[65])
{
	ULONG idx = 0;
	DWORD dwRetVal = 0;
	ULONG outBufLen = 0;
	WCHAR wAdapterName[256] = {0};
	PIP_PER_ADAPTER_INFO pPerAdapterInfo;
	
	MultiByteToWideChar(CP_ACP, 0, name, -1, wAdapterName, 256);
	if(NO_ERROR!=GetAdapterIndex(wAdapterName, &idx))
		return -1;

	outBufLen = sizeof(IP_PER_ADAPTER_INFO);
	pPerAdapterInfo = (PIP_PER_ADAPTER_INFO)malloc(outBufLen);
	if(ERROR_BUFFER_OVERFLOW==GetPerAdapterInfo(idx, pPerAdapterInfo, &outBufLen))
	{
		free(pPerAdapterInfo);
		pPerAdapterInfo = (PIP_PER_ADAPTER_INFO)malloc(outBufLen);
	}

	if ((dwRetVal = GetPerAdapterInfo(idx, pPerAdapterInfo, &outBufLen)) == ERROR_SUCCESS)
	{
		secondary[0] = '\0';
		strncpy(primary, pPerAdapterInfo->DnsServerList.IpAddress.String, 39);
		if(pPerAdapterInfo->DnsServerList.Next)
			strncpy(secondary, pPerAdapterInfo->DnsServerList.Next->IpAddress.String, 39);
	}

	free(pPerAdapterInfo);
	return dwRetVal==ERROR_SUCCESS?0:-(int)dwRetVal;
}

int network_setdns(const char* name, const char* primary, const char *secondary)
{
	HKEY hKey;
	char key[MAX_PATH];
	char dns[MAX_PATH] = {0};

	if(VALIDATE_IPADDR(primary))
	{
		strcpy(dns, primary);
	}

	if(VALIDATE_IPADDR(secondary))
	{
		if(*dns) strcat(dns, ",");
		strcat(dns, secondary);
	}

	strcpy(key, "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\");
	strcat(key, name);

	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
		return -1;

	RegSetValueExA(hKey, "NameServer", 0, REG_SZ, (BYTE*)(const char*)dns, strlen(dns));
	RegCloseKey(hKey);

	// flush dns
	network_notify_dnsflush();
	return 0;
}

int network_getgateway(char gateway[65])
{
	// http://msdn.microsoft.com/en-us/library/aa373798%28v=VS.85%29.aspx
	return -1;
}

int network_setgateway(const char* gateway)
{
	// http://msdn.microsoft.com/en-us/library/aa373798%28v=VS.85%29.aspx
	return -1;
}

#else

int network_getgateway(char gateway[65])
{
	FILE* fp;
	char buffer[512];

	fp = popen("ip route", "r");
	if(!fp)
		return -(int)errno;

	while(fgets(buffer, sizeof(buffer), fp))
	{
		if(1 == sscanf(buffer, "default via %65s dev %*s", gateway))
			break;
	}
	pclose(fp);
	return 0;
}

int network_setgateway(const char* gateway)
{
	int r;
	char buffer[128];

	strcpy(buffer, "route del default gw ");
	r = (int)strlen(buffer);

	r = network_getgateway(buffer+r);
	if(0 == r)
	{
		system(buffer);
	}

	sprintf(buffer, "route add default gw %s", gateway);
	r = system(buffer);
	return 0; // system always return -1
}

static void ipaddr2str(char s[16], struct sockaddr_in* addr)
{
	sprintf(s, "%u.%u.%u.%u", 
		addr->sin_addr.s_addr & 0xFF,
		(addr->sin_addr.s_addr>>8) & 0xFF,
		(addr->sin_addr.s_addr>>16) & 0xFF,
		(addr->sin_addr.s_addr>>24) & 0xFF);
}

#if 0
int network_getip(network_getip_fcb fcb, void* param)
{
	int i, fd;
	struct ifconf ifc;
	struct ifreq req[16];
	char hwaddr[20];	
	char ipaddr[32];
	char netmask[32];
	char gateway[65];

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd < 0)
		return -(int)errno;

	ifc.ifc_req = req;
	ifc.ifc_len = sizeof(req);	
	if(ioctl(fd, SIOCGIFCONF, (char*)&ifc) < 0)
	{
		close(fd);
		return -(int)errno;
	}

	for(i=0; i<ifc.ifc_len/sizeof(req[0]); i++)
	{
		if(0 == strcmp(req[i].ifr_name, "lo"))
			continue; // local loopback

		if(ioctl(fd, SIOCGIFHWADDR, (char*)&req[i]) < 0)
			continue;

		sprintf(hwaddr, "%02X-%02X-%02X-%02X-%02X-%02X",
				(int)(unsigned char)req[i].ifr_hwaddr.sa_data[0],
				(int)(unsigned char)req[i].ifr_hwaddr.sa_data[1],
				(int)(unsigned char)req[i].ifr_hwaddr.sa_data[2],
				(int)(unsigned char)req[i].ifr_hwaddr.sa_data[3],
				(int)(unsigned char)req[i].ifr_hwaddr.sa_data[4],
				(int)(unsigned char)req[i].ifr_hwaddr.sa_data[5]);

		if(ioctl(fd, SIOCGIFADDR, (char*)&req[i]) < 0)
			continue;
		ipaddr2str(ipaddr, (struct sockaddr_in*)&req[i].ifr_addr);

		if(ioctl(fd, SIOCGIFNETMASK, (char*)&req[i]) < 0)
			continue;
		ipaddr2str(netmask, (struct sockaddr_in*)&req[i].ifr_netmask);

		memset(gateway, 0, sizeof(gateway));
		network_getgateway(gateway);
		fcb(param, hwaddr, req[i].ifr_name, 0, ipaddr, netmask, gateway);
	}

	close(fd);
	return 0;
}

#else
#if !defined(OS_MAC)
static int network_getmac(const struct ifaddrs *ifaddr, const char* ifname, char hwaddr[20])
{
	const struct ifaddrs *ifa = NULL;
	struct sockaddr_ll *macaddr = NULL;

	for(ifa = ifaddr; ifa; ifa = ifa->ifa_next)
	{
		if(0 == strcmp(ifa->ifa_name, ifname) 
			&& AF_PACKET == ifa->ifa_addr->sa_family
			&& ifa->ifa_addr )
		{
			macaddr = (struct sockaddr_ll*)ifa->ifa_addr;
			sprintf(hwaddr, "%02X-%02X-%02X-%02X-%02X-%02X",
				(unsigned int)(macaddr->sll_addr[0]),
				(unsigned int)(macaddr->sll_addr[1]),
				(unsigned int)(macaddr->sll_addr[2]),
				(unsigned int)(macaddr->sll_addr[3]),
				(unsigned int)(macaddr->sll_addr[4]),
				(unsigned int)(macaddr->sll_addr[5]));
			return 0;
		}
	}
	return -1;
}
#endif

int network_getip(network_getip_fcb fcb, void* param)
{
	char hwaddr[20];	
	char ipaddr[32] ,netmask[32], gateway[65];
	struct ifaddrs *ifaddr, *ifa;

	memset(gateway, 0, sizeof(gateway));
	network_getgateway(gateway);

	if(0 != getifaddrs(&ifaddr))
		return -errno;

	for(ifa = ifaddr; ifa; ifa = ifa->ifa_next)
	{
		if(!ifa->ifa_addr || AF_INET != ifa->ifa_addr->sa_family || 0 == strcmp("lo", ifa->ifa_name))
			continue;

		//case AF_INET6:
		//	struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)ifa->ifa_addr;
		//	struct sockaddr_in6* mask = (struct sockaddr_in6*)ifa->ifa_netmask;
		//	break;

		ipaddr2str(ipaddr, (struct sockaddr_in*)ifa->ifa_addr);
		ipaddr2str(netmask, (struct sockaddr_in*)ifa->ifa_netmask);

		memset(hwaddr, 0, sizeof(hwaddr));
#if !defined(OS_MAC)
		network_getmac(ifaddr, ifa->ifa_name, hwaddr);
#endif

		fcb(param, hwaddr, ifa->ifa_name, 0, ipaddr, netmask, gateway);
	}

	freeifaddrs(ifaddr);
	return 0;
}
#endif

#if 0
int network_setip(const char* name, int enableDHCP, const char* ip, const char* netmask, const char* gateway)
{
	int fd;
	struct ifreq req;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd < 0)
		return -(int)errno;

	strcpy(req.ifr_name, name);

	((struct sockaddr_in*)&req.ifr_addr)->sin_port = 0;
	((struct sockaddr_in*)&req.ifr_addr)->sin_family = AF_INET;	
	((struct sockaddr_in*)&req.ifr_addr)->sin_addr.s_addr = inet_addr(ip);
	if(ioctl(fd, SIOCSIFADDR, (char*)&req) < 0)
	{
		close(fd);
		return -(int)errno;
	}

	((struct sockaddr_in*)&req.ifr_netmask)->sin_addr.s_addr = inet_addr(netmask);
	if(ioctl(fd, SIOCSIFNETMASK, (char*)&req) < 0)
	{
		close(fd);
		return -(int)errno;
	}

	// net up and running
	if(0 == ioctl(fd, SIOCGIFFLAGS, &req))
	{
		req.ifr_flags |= IFF_UP | IFF_RUNNING;
		ioctl(fd, SIOCSIFFLAGS, &req);
	}

	close(fd);

	if(gateway && *gateway)
		network_setgateway(gateway);
	return 0;
}

#else
static int network_setip_handle(const char* str, int len, va_list val)
{
	FILE* fp;
	
	if(len < 1)
		return 0;

	if(strncmp("NETMASK", str, strlen("NETMASK"))
		&& strncmp("IPADDR", str, strlen("IPADDR"))
		&& strncmp("GATEWAY", str, strlen("GATEWAY"))
		&& strncmp("BOOTPROTO", str, strlen("BOOTPROTO")))
	{
		fp = va_arg(val, FILE*);
		fwrite(str, 1, len, fp);
	}

	return 0;
}

int network_setip(const char* name, int dhcp, const char* ip, const char* netmask, const char* gateway)
{
	int r;
	char filename[512] = {0};
	char content[1024*2] = {0};

	// read
	sprintf(filename, "/etc/sysconfig/network-scripts/ifcfg-%s", name);
	tools_cat(filename, content, sizeof(content)-1);

	// update
	FILE* fp = fopen(filename, "w");
	if(!fp)
		return -(int)errno;

	r = tools_tokenline(content, network_setip_handle, fp);

	if(dhcp)
		sprintf(content, "BOOTPROTO=dhcp\n");
	else
		sprintf(content, "BOOTPROTO=static\nIPADDR=%s\nNETMASK=%s\nGATEWAY=%s\n", ip?ip:"", netmask?netmask:"", gateway?gateway:"");

	fwrite(content, 1, strlen(content), fp);
	fclose(fp);

	if(gateway && *gateway)
		r = network_setgateway(gateway);
	return r;
}
#endif

static int network_getdns_handle(const char* str, int strLen, va_list val)
{
	char *dns;
	int v[4];

	// TODO: IPv6

	if(strLen > 0 && 4 == sscanf(str, "nameserver %d:%d:%d:%d", &v[0], &v[1], &v[2], &v[3]))
	{
		if(0 <= v[0] && v[0] <= 255 && 0 <= v[1] && v[1] <= 255 && 0 <= v[2] && v[2] <= 255 && 0 <= v[3] && v[3] <= 255)
		{
			dns = va_arg(val, char*);
			if(NULL == dns)
				return 1; // stop

			sprintf(dns, "%d:%d:%d:%d", v[0], v[1], v[2], v[3]);
		}
	}
	return 0;
}

int network_getdns(const char* name, char primary[65], char secondary[65])
{
	int r;
	char content[1024*2] = {0};

	r = tools_cat("/etc/resolv.conf", content, sizeof(content)-1);
	r = tools_tokenline(content, network_getdns_handle, primary, secondary, NULL);
	return r;
}

static int network_setdns_handle(const char* str, int len, va_list val)
{
	FILE* fp;

	if(len < 1)
		return 0;

	if(strncmp(str, "nameserver ", 11))
	{
		fp = va_arg(val, FILE*);
		fwrite(str, 1, len, fp);
	}

	return 0;
}

int network_setdns(const char* name, const char* primary, const char *secondary)
{
	int r;
	FILE* fp;
	char content[1024*3] = {0};	

	// read
	tools_cat("/etc/resolv.conf", content, sizeof(content)-1);

	fp = fopen("/etc/resolv.conf", "w");
	if(!fp)
		return -(int)errno;

	// update
	r = tools_tokenline(content, network_setdns_handle, fp);

	if(primary && *primary)
	{
		snprintf(content, sizeof(content), "nameserver %s\n", primary);
		fwrite(content, 1, strlen(content), fp);
	}

	if(secondary && *secondary)
	{
		snprintf(content, sizeof(content), "nameserver %s\n", secondary);
		fwrite(content, 1, strlen(content), fp);
	}

	fclose(fp);
	return r;
}

#endif
