#include "sysnetconfig.h"
#include "cstringext.h"
#include "tools.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#include <IPHlpApi.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Mpr.lib")

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
#include <net/if.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
//int system_netadaptors_count()
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

int system_getip(system_getip_fcb fcb, void* param)
{
	UINT i = 0;
	ULONG ulOutBufLen;
	DWORD dwRetVal = 0;
	PIP_ADAPTER_INFO pAdapter, pAdapterInfo;
	char hwaddr[MAX_ADAPTER_ADDRESS_LENGTH*3] = {0};

	// Make an initial call to GetAdaptersInfo to get
	// the necessary size into the ulOutBufLen variable
	ulOutBufLen = sizeof(IP_ADAPTER_INFO);
	pAdapterInfo = (IP_ADAPTER_INFO *)malloc(ulOutBufLen);
	if (GetAdaptersInfo( pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) 
	{
		free(pAdapterInfo);
		pAdapterInfo = (IP_ADAPTER_INFO *) malloc (ulOutBufLen); 
	}

	if ((dwRetVal = GetAdaptersInfo( pAdapterInfo, &ulOutBufLen)) == ERROR_SUCCESS)
	{
		pAdapter = pAdapterInfo;
		while(pAdapter)
		{
			if(pAdapter->Type == MIB_IF_TYPE_ETHERNET)
			{
				// mac address
				for(i=0; i<pAdapter->AddressLength; i++)
				{
					if(i > 0) hwaddr[i*3-1] = ':';
					sprintf(hwaddr+i*3, "%02X", (int)pAdapter->Address[i]);
				}

				fcb(param, 
					hwaddr,										// mac address
					pAdapter->AdapterName,						// name
					0==pAdapter->DhcpEnabled?0:1,				// dhcp
					pAdapter->IpAddressList.IpAddress.String,	// ip address
					pAdapter->IpAddressList.IpMask.String,		// netmask
					pAdapter->GatewayList.IpAddress.String);	// gateway
			}
			pAdapter = pAdapter->Next;
		}
	}

	free(pAdapterInfo);
	return dwRetVal==ERROR_SUCCESS?0:-(int)dwRetVal;
}

typedef int (CALLBACK* DNSFLUSHPROC)();
typedef int (CALLBACK* DHCPNOTIFYPROC)(LPWSTR, LPWSTR, BOOL, DWORD, DWORD, DWORD, int);
static int system_notify_ipchanged(const char* name, const char* ip, const char* netmask, int dhcp)
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

static int system_notify_dnsflush()
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

int system_setip(const char* name, int enableDHCP, const char* ipaddr, const char* netmask, const char* gateway)
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
	system_notify_ipchanged(name, ipaddr, netmask, enableDHCP);
	return 0;
}

int system_getdns(const char* name, char dns1[128], char dns2[128])
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
		strcpy(dns1, pPerAdapterInfo->DnsServerList.IpAddress.String);
		if(pPerAdapterInfo->DnsServerList.Next)
			strcpy(dns2, pPerAdapterInfo->DnsServerList.Next->IpAddress.String);
	}

	free(pPerAdapterInfo);
	return dwRetVal==ERROR_SUCCESS?0:-(int)dwRetVal;
}

int system_setdns(const char* name, const char* dns1, const char* dns2)
{
	HKEY hKey;
	char key[MAX_PATH];
	char dns[MAX_PATH] = {0};

	if(VALIDATE_IPADDR(dns1))
	{
		strcpy(dns, dns1);
	}

	if(VALIDATE_IPADDR(dns2))
	{
		if(*dns) strcat(dns, ",");
		strcat(dns, dns2);
	}

	strcpy(key, "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\");
	strcat(key, name);

	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
		return -1;

	RegSetValueExA(hKey, "NameServer", 0, REG_SZ, (BYTE*)(const char*)dns, strlen(dns));
	RegCloseKey(hKey);

	// flush dns
	system_notify_dnsflush();
	return 0;
}

int system_getgateway(char* gateway, int len)
{
	// http://msdn.microsoft.com/en-us/library/aa373798%28v=VS.85%29.aspx
	return -1;
}

int system_setgateway(const char* gateway)
{
	// http://msdn.microsoft.com/en-us/library/aa373798%28v=VS.85%29.aspx
	return -1;
}

#else

int system_getgateway(char* gateway, int len)
{
	FILE* fp;
	char buffer[512];
	const char* p;

	fp = popen("ip route", "r");
	if(!fp)
		return -(int)errno;

	while(fgets(buffer, sizeof(buffer), fp))
	{
		p = skips(buffer, " \r\n");
		if(strneq("default ", p, 8))
		{
			sscanf(p, "%*s %*s %s", gateway);
			break;
		}
	}
	pclose(fp);
	return 0;
}

int system_setgateway(const char* gateway)
{
	int r;
	char buffer[128];

	strcpy(buffer, "route del default gw ");
	r = strlen(buffer);

	r = system_getgateway(buffer+r, sizeof(buffer)-r);
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
	sprintf(s, "%d.%d.%d.%d", 
		addr->sin_addr.s_addr & 0xFF,
		(addr->sin_addr.s_addr>>8) & 0xFF,
		(addr->sin_addr.s_addr>>16) & 0xFF,
		(addr->sin_addr.s_addr>>24) & 0xFF);
}

int system_getip(system_getip_fcb fcb, void* param)
{
	int i, fd;
	struct ifconf ifc;
	struct ifreq req[16];
	char hwaddr[20];	
	char ipaddr[32];
	char netmask[32];
	char gateway[32];

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
		system_getgateway(gateway, sizeof(gateway));
		fcb(param, hwaddr, req[i].ifr_name, 0, ipaddr, netmask, gateway);
	}

	close(fd);
	return 0;
}

#if 0
int system_setip(const char* name, int enableDHCP, const char* ip, const char* netmask, const char* gateway)
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
		system_setgateway(gateway);
	return 0;
}

#else

static int system_setip_handle(const char* str, int len, va_list val)
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

int system_setip(const char* name, int dhcp, const char* ip, const char* netmask, const char* gateway)
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

	r = tools_tokenline(content, system_setip_handle, fp);

	if(dhcp)
		sprintf(content, "BOOTPROTO=dhcp\n");
	else
		sprintf(content, "BOOTPROTO=static\nIPADDR=%s\nNETMASK=%s\nGATEWAY=%s\n", ip?ip:"", netmask?netmask:"", gateway?gateway:"");

	fwrite(content, 1, strlen(content), fp);
	fclose(fp);

	if(gateway && *gateway)
		r = system_setgateway(gateway);
	return r;
}
#endif

static int system_getdns_handle(const char* str, int strLen, va_list val)
{
	int len, n;
	char *dns;
	const char *p;

	if(0 == strncmp("nameserver ", str, 11))
	{
		dns = va_arg(val, char*);
		len = va_arg(val, int);

		n = strlen(dns);
		if(n > 0)
		{
			dns[n] = ';';
			dns += n+1;
			len -= 1;
		}
		len -= n;

		p = skip(str+10, ' ');
		token(p, "\r\n ", dns, len);
	}
	return 0;
}

int system_getdns(const char* name, char* dns, int dnsLen)
{
	int r;
	char content[1024*2] = {0};

	r = tools_cat("/etc/resolv.conf", content, sizeof(content)-1);
	r = tools_tokenline(content, system_getdns_handle, dns, dnsLen);
	return r;
}

static int system_setdns_handle(const char* str, int len, va_list val)
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

int system_setdns(const char* name, const char* dns)
{
	int r;
	FILE* fp;
	char content[1024*3] = {0};	
	const char *line, *p;
	
	// read
	tools_cat("/etc/resolv.conf", content, sizeof(content)-1);

	fp = fopen("/etc/resolv.conf", "w");
	if(!fp)
		return -(int)errno;

	// update
	r = tools_tokenline(content, system_setdns_handle, fp);

	line = dns;
	while(line && *line)
	{
		line = skips(line, " ;");
		p = strchr(line, ';');
		if(p)
		{
			assert(p > line);
			fwrite("nameserver ", 1, 11, fp);
			fwrite(line, 1, p-line, fp);
			fwrite("\n", 1, 1, fp);
		}
		else
		{
			sprintf(content, "nameserver %s\n", line);
			fwrite(content, 1, strlen(content), fp);
			break;
		}
		line = p+1;
	}

	fclose(fp);
	return r;
}

#endif
