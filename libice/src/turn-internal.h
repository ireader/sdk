#ifndef _turn_internal_h_
#define _turn_internal_h_

#include "list.h"
#include "darray.h"
#include "sockutil.h"
#include "stun-internal.h"
#include <stdint.h>

struct turn_permission_t
{
	struct sockaddr_storage addr; // Note that only addresses are compared and port numbers are not considered.
	uint64_t expired; // expired clock
};

struct turn_channel_t
{
	struct sockaddr_storage addr;
	uint16_t channel;
	uint64_t expired; // expired clock
};

// rfc 5766 5. Allocations (p22)
struct turn_allocation_t
{
	struct list_head link;
	uint16_t id[16];

	int transport; // STUN_PROTOCOL_UDP
	int peertransport;
	struct sockaddr_storage relay;
	struct sockaddr_storage client;
	struct sockaddr_storage server;

	// By default, each Allocate or Refresh transaction resets this
	// timer to the default lifetime value of 600 seconds (10 minutes)
	uint64_t expire; // time-to-expiry

	uint32_t dontfragment;
	
	// authentication;
	struct stun_credetial_t auth;

	struct darray_t permissions;
	struct darray_t channels;
};

int turn_agent_onallocate(const struct stun_transaction_t* req, struct stun_transaction_t* resp);
int turn_agent_onrefresh(const struct stun_transaction_t* req, struct stun_transaction_t* resp);
int turn_agent_oncreate_permission(const struct stun_transaction_t* req, struct stun_transaction_t* resp);
int turn_agent_onchannel_bind(const struct stun_transaction_t* req, struct stun_transaction_t* resp);
int turn_agent_onsend(const struct stun_transaction_t* req, struct stun_transaction_t* resp);
int turn_agent_ondata(const struct stun_transaction_t* req, struct stun_transaction_t* resp);

#endif /* _turn_internal_h_ */
