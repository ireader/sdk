#include "port/ip-route.h"
#include "sys/sock.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "lwip/ip.h"
#include "lwip/netif.h"

static int router_gateway(const struct sockaddr* dst, struct sockaddr_storage* gateway) {
    struct netif *netif;
    const ip4_addr_t *gw;

    // Check if the destination address is IPv4
    if (dst->sa_family == AF_INET) {
        const struct sockaddr_in *dst_in = (const struct sockaddr_in *)dst;
        ip4_addr_t ip4addr = *(ip4_addr_t*)&dst_in->sin_addr;

        // Find the netif for the destination address
        netif = ip4_route(&ip4addr);

        if (netif != NULL) {
            // Get the gateway address for the found netif
            gw = netif_ip4_gw(netif);

            if (gw != NULL) {
                // Store the gateway address in the sockaddr_storage structure
                struct sockaddr_in *gw_in = (struct sockaddr_in *)gateway;
                gw_in->sin_family = AF_INET;
                gw_in->sin_port = 0;
                gw_in->sin_addr = *(struct in_addr*)gw;
                memset(gw_in->sin_zero, 0, sizeof(gw_in->sin_zero));

                return 0; // Success
            }
        }
    } else if (dst->sa_family == AF_INET6) {
        // IPv6 is not handled in this example
        return -1; // Not implemented
    }

    return -1; // Failure
}
