#include "port/ip-route.h"
#include "sys/sock.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#if !defined(__ANDROID_API__) || __ANDROID_API__ >= 24
#include <ifaddrs.h>
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static inline int hweight32(uint32_t w)
{
    w = w - ((w >> 1) & 0x55555555);
    w = (w & 0x33333333) + ((w >> 2) & 0x33333333);
    w = (w + (w >> 4)) & 0x0F0F0F0F;
    w = (w + (w >> 8));
    return (w + (w >> 16)) & 0x000000FF;
}

static int router_iproute(const char* distination, char ip[40])
{
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
    
    if(n < 40 && socket_isip(cmd))
    {
        strncpy(ip, cmd, n);
        ip[n] = '\0';
        return 0;
    }
    return -1;
}

#if !defined(__ANDROID_API__) || __ANDROID_API__ >= 24
static int router_iface_addr(const char* iface, struct sockaddr_storage* addr)
{
    int r;
    struct ifaddrs *ifaddr, *ifa;
    
    r = getifaddrs(&ifaddr);
    if(0 != r)
        return r;
    
    for(ifa = ifaddr; ifa; ifa = ifa->ifa_next)
    {
        if(!ifa->ifa_addr  || 0 != strcmp(iface, ifa->ifa_name))
            continue;
        
        if(AF_INET != ifa->ifa_addr->sa_family && AF_INET6 != ifa->ifa_addr->sa_family)
            continue;
        
        memcpy(addr, ifa->ifa_addr, socket_addr_len(ifa->ifa_addr));
        break;
    }
    
    freeifaddrs(ifaddr);
    return 0;
}

#else
static int router_iface_addr(const char* iface, struct sockaddr_storage* addr)
{
    int sockfd;
    struct ifreq ifr;
    
    sockfd = socket(addr->ss_family, SOCK_DGRAM, 0);
    if (sockfd == -1)
        return -1;
    
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_addr.sa_family = addr->ss_family;
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", iface);
    
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) == -1)
    {
        close(sockfd);
        return -1;
    }
    
    memcpy(addr, &ifr.ifr_addr, socket_addr_len(&ifr.ifr_addr));
    close(sockfd);
    return 0;
}
#endif

/*
 [root@localhost net]# cat route
 Iface    Destination    Gateway     Flags    RefCnt    Use    Metric    Mask        MTU    Window    IRTT
 eth0    000CA8C0    00000000    0001    0        0    0        00FFFFFF    0    0    0
 eth0    0000FEA9    00000000    0001    0        0    0        0000FFFF    0    0    0
 eth0    00000000    010CA8C0    0003    0        0    0        00000000    0    0    0
 */
static int router_proc(uint32_t peer, char iface[64])
{
    FILE* fp;
    int n, score;
    char name[18], line[1024];
    uint32_t destination, gateway, netmask;
    
    fp = fopen("/proc/net/route", "r");
    if (!fp)
        return -1;
    
    score = -1;
    name[0] = iface[0] = '\0';
    fgets(line, sizeof(line), fp); // filter first line
    
    while (NULL != fgets(line, sizeof(line), fp))
    {
        if (4 == sscanf(line, "%16s %X %X %*X %*d %*d %*d %X", name, &destination, &gateway, &netmask))
        {
            assert((destination & netmask) == destination);
            if (0 == destination && -1 == score)
            {
                // default gateway
                score = 0;
                strcpy(iface, name);
            }
            else if ((peer & netmask) == destination)
            {
                // found
                n = hweight32(netmask);
                if (n > score)
                {
                    score = n;
                    strcpy(iface, name);
                }
            }
            else if (-1 == score)
            {
                // don't have default gateway
                strcpy(iface, name); // fall-through
            }
        }
    }
    
    fclose(fp);
    return 0;
}

static int router_gateway(const struct sockaddr* dst, struct sockaddr_storage* gateway)
{
    char iface[64];
    if(AF_INET != dst->sa_family || 0 != router_proc(((const struct sockaddr_in*)dst)->sin_addr.s_addr, iface))
        return -1;
    
    gateway->ss_family = dst->sa_family;
    return router_iface_addr(iface, gateway);
}
