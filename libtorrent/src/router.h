#ifndef _router_h_
#define _router_h_

#include <stdint.h>
#include <stddef.h>
#include "node.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct router_t;

struct router_t* router_create(const uint8_t id[N_NODEID]);
void router_destroy(struct router_t* router);

int router_add(struct router_t* router, const uint8_t id[N_NODEID], const struct sockaddr_storage* addr, struct node_t** node);
int router_remove(struct router_t* router, const uint8_t id[N_NODEID]);
size_t router_size(struct router_t* router);

int router_nearest(struct router_t* router, const uint8_t id[N_NODEID], struct node_t* nodes[], size_t count);

int router_list(struct router_t* router, int (*func)(void* param, struct node_t* node), void* param);

#if defined(__cplusplus)
}
#endif
#endif /* !_router_h_ */
