#ifndef _sockpair_h_
#define _sockpair_h_

#include "sys/sock.h"

#if defined(__cplusplus)
extern "C" {
#endif

void sockpair_set_port_range(unsigned short base, unsigned short num);
void sockpair_get_port_range(unsigned short *base, unsigned short *num);

int sockpair_create(const char* ip, socket_t pair[2], unsigned short port[2]);
int sockpair_create2(const struct sockaddr* addr, socket_t pair[2], unsigned short port[2]);

#if defined(__cplusplus)
}
#endif
#endif /* !_sockpair_h_ */
