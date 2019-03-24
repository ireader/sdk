#ifndef _turn_internal_h_
#define _turn_internal_h_

#include "list.h"
#include "sockutil.h"
#include "stun-internal.h"
#include <stdint.h>

struct turn_permission_t
{
	struct list_head link;
	struct sockaddr_storage addr; // Note that only addresses are compared and port numbers are not considered.
	uint64_t expired; // expired clock
};

// rfc 5766 5. Allocations (p22)
struct turn_allocate_t
{
	struct list_head link;
	uint16_t id[16];

	int transport; // STUN_PROTOCOL_UDP
	struct sockaddr_storage relay;
	struct sockaddr_storage client;
	struct sockaddr_storage server;

	// By default, each Allocate or Refresh transaction resets this
	// timer to the default lifetime value of 600 seconds (10 minutes)
	uint32_t lifetime; // default 600s ~ 3600s
	int permissions;
	int channels;

	// authentication;
	struct stun_credetial_t auth;
};

#endif /* _turn_internal_h_ */
