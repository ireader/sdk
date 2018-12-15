// https://opensource.apple.com/source/network_cmds/network_cmds-543.50.4/netstat.tproj

#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "net/route.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

#ifndef SA_SIZE
#if defined(__OpenBSD__) || defined(__FreeBSD__)
#define SA_SIZE(sa) (  (!(sa) || ((struct sockaddr *)(sa))->sa_len == 0) ?  \
                            sizeof(long)     :               \
                            1 + ( (((struct sockaddr *)(sa))->sa_len - 1) | (sizeof(long) - 1) ) )
#else
#define SA_SIZE(sa) (  (!(sa) || ((struct sockaddr *)(sa))->sa_len == 0) ?  \
                            sizeof(int)     :               \
                            1 + ( (((struct sockaddr *)(sa))->sa_len - 1) | (sizeof(int) - 1) ) )
#endif
#endif

static void router_getaddrs(struct rt_msghdr * rtm, struct sockaddr* addrs[RTAX_MAX])
{
    int i;
    struct sockaddr* sa;
    sa = (struct sockaddr*)(rtm + 1);
    
    for(i = 0; i < RTAX_MAX; i++)
    {
        if(rtm->rtm_addrs & (1 << i))
        {
            addrs[i] = sa;
            sa = (struct sockaddr *)((char *)sa + SA_SIZE(sa)); // next addr
        }
        else
        {
            addrs[i] = NULL;
        }
    }
}
static int router_gateway(const struct sockaddr* dst, struct sockaddr_storage* gateway)
{
    int               sockfd;
    char            buf[512];
    pid_t                pid;
    ssize_t                r;
    struct rt_msghdr    *rtm;
    struct sockaddr*    sa, *addrs[RTAX_MAX];
    
    sockfd = socket(AF_ROUTE, SOCK_RAW, 0);    /* need superuser privileges */
    
    pid = getpid();
    memset(buf, 0, sizeof(buf));
    rtm = (struct rt_msghdr *) buf;
    rtm->rtm_msglen = sizeof(struct rt_msghdr) + dst->sa_len;
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_type = RTM_GET;
    rtm->rtm_addrs = RTA_DST|RTA_IFA;
    rtm->rtm_pid = pid;
    
    sa = (struct sockaddr *) (rtm + 1);
    memcpy(sa, dst, dst->sa_len);
    
    r = write(sockfd, rtm, rtm->rtm_msglen);
    if(r < 0)
    {
        close(sockfd);
        return (int)r;
    }
    
    do {
        r = read(sockfd, buf, sizeof(buf));
    } while (rtm->rtm_type != RTM_GET || rtm->rtm_pid != pid  /*|| rtm->rtm_seq != SEQ*/);
    /* end getrt1 */
    
    /* include getrt2 */
    router_getaddrs((struct rt_msghdr *) buf, addrs);
    if(addrs[RTAX_IFA])
        memcpy(gateway, addrs[RTAX_IFA], addrs[RTAX_IFA]->sa_len);
    else if(addrs[RTAX_GATEWAY])
        memcpy(gateway, addrs[RTAX_GATEWAY], addrs[RTAX_GATEWAY]->sa_len);
    
    close(sockfd);
    return 0;
}

static inline int ipv4_addr(struct sockaddr * sa, char line[MAXHOSTNAMELEN])
{
    struct sockaddr_in *sockin;
    struct sockaddr_in6 *sockin6;
    
    if(sa->sa_family == AF_INET)
    {
        sockin = (struct sockaddr_in *)sa;
        inet_ntop(AF_INET, &sockin->sin_addr.s_addr, line, MAXHOSTNAMELEN - 1);
    }
    else if(sa->sa_family == AF_INET)
    {
        sockin6 = (struct sockaddr_in6 *)sa;
        inet_ntop(AF_INET6, &sockin6->sin6_addr.s6_addr, line, MAXHOSTNAMELEN - 1);
    }
    else
    {
        return -1;
    }
    
    return 0;
}

static int router_list(void)
{
    size_t needed;
    int mib[6];
    char *buf, *next, *lim;
    struct rt_msghdr *rtm;
    struct sockaddr *sa;
    char dst[MAXHOSTNAMELEN];
    char gateway[MAXHOSTNAMELEN];
    char netmask[MAXHOSTNAMELEN];
    
    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = 0;
    mib[4] = NET_RT_DUMP;
    mib[5] = 0;
    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
        return -1;
    }
    
    if ((buf = (char *)malloc(needed)) == NULL) {
        return -1;
    }
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
        free(buf);
        return -1;
    }
    
    lim  = buf + needed;
    for (next = buf; next < lim; next += rtm->rtm_msglen) {
        rtm = (struct rt_msghdr *)next;
        if(RTM_GET != rtm->rtm_type)
            continue;
        
        memset(dst, 0, sizeof(dst));
        memset(gateway, 0, sizeof(gateway));
        memset(netmask, 0, sizeof(netmask));
        sa = (struct sockaddr *)(rtm + 1);
        if(RTA_DST & rtm->rtm_addrs)
        {
            ipv4_addr(sa, dst);
            sa = (struct sockaddr *)(SA_SIZE(sa) + (char *)sa);
        }
        if(RTA_GATEWAY & rtm->rtm_addrs)
        {
            ipv4_addr(sa, gateway);
            sa = (struct sockaddr *)(SA_SIZE(sa) + (char *)sa);
        }
#if defined(OS_MAC)
        snprintf(netmask, sizeof(netmask), "%u.%u.%u.%u",
                 (unsigned int)((unsigned char*)sa)[0],
                 (unsigned int)((unsigned char*)sa)[1],
                 (unsigned int)((unsigned char*)sa)[2],
                 (unsigned int)((unsigned char*)sa)[3]);
#else
        if(RTA_NETMASK & rtm->rtm_addrs)
        {
            if(RTA_DST & rtm->rtm_addrs && 0 == sa->sa_family)
                sa->sa_family = AF_INET;
            ipv4_addr(sa, netmask);
            sa = (struct sockaddr *)(SA_SIZE(sa) + (char *)sa);
        }
#endif
        
        printf("dst: %s, gateway=%s, netmask: %s, addr: %X, flags: %X\n", dst, gateway, netmask, rtm->rtm_addrs, rtm->rtm_flags);
    }
    
    free(buf);
    return 0;
}
