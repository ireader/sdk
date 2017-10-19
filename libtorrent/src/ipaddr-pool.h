#ifndef _peer_pool_h_
#define _peer_pool_h_

#include "sys/sock.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct ipaddr_pool_t ipaddr_pool_t;

ipaddr_pool_t* ipaddr_pool_create();
void ipaddr_pool_destroy(ipaddr_pool_t* pool);
void ipaddr_pool_clear(ipaddr_pool_t* pool);

int ipaddr_pool_empty(ipaddr_pool_t* pool);
size_t ipaddr_pool_size(ipaddr_pool_t* pool);

/// @return 0-ok, other-error
int ipaddr_pool_push(ipaddr_pool_t* pool, const struct sockaddr_storage* addr);
int ipaddr_pool_pop(ipaddr_pool_t* pool, struct sockaddr_storage* addr);

#if defined(__cplusplus)
}
#endif
#endif /* !_peer_pool_h_ */
