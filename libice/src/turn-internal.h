#ifndef _turn_internal_h_
#define _turn_internal_h_

#include "list.h"
#include "sockutil.h"
#include "stun-internal.h"
#include <stdint.h>

struct stun_address_t;

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
//	uint16_t id[16];

	int peertransport;
	struct stun_address_t addr; // client-mode: local address, server-mode: remote address

	// By default, each Allocate or Refresh transaction resets this
	// timer to the default lifetime value of 600 seconds (10 minutes)
	uint64_t expire; // time-to-expiry
	uint32_t lifetime; // seconds

	int reserve_next_higher_port; // EVEN-PORT
	uint8_t token[8];
	uint32_t dontfragment;

	// authentication;
	struct stun_credential_t auth;
	
	int npermission;
	struct turn_permission_t permissions[8];

	int nchannel;
	struct turn_channel_t channels[8];
};


// The port portion of each attribute is ignored
static inline int turn_sockaddr_cmp(const struct sockaddr* l, const struct sockaddr* r)
{
	if (AF_INET == l->sa_family && AF_INET == r->sa_family)
		return memcmp(&((struct sockaddr_in*)l)->sin_addr, &((struct sockaddr_in*)r)->sin_addr, sizeof(((struct sockaddr_in*)r)->sin_addr));
	if (AF_INET6 == l->sa_family && AF_INET6 == r->sa_family)
		return memcmp(&((struct sockaddr_in6*)l)->sin6_addr, &((struct sockaddr_in6*)r)->sin6_addr, sizeof(((struct sockaddr_in6*)r)->sin6_addr));
	return -1;
}

int turn_server_onallocate(struct stun_agent_t* turn, const struct stun_request_t* req, struct stun_response_t* resp);
int turn_server_onrefresh(struct stun_agent_t* turn, const struct stun_request_t* req, struct stun_response_t* resp);
int turn_server_oncreate_permission(struct stun_agent_t* turn, const struct stun_request_t* req, struct stun_response_t* resp);
int turn_server_onchannel_bind(struct stun_agent_t* turn, const struct stun_request_t* req, struct stun_response_t* resp);

int turn_client_allocate_onresponse(struct stun_agent_t* stun, struct stun_request_t* req, const struct stun_message_t* resp);
int turn_client_refresh_onresponse(struct stun_agent_t* stun, struct stun_request_t* req, const struct stun_message_t* resp);
int turn_client_create_permission_onresponse(struct stun_agent_t* stun, struct stun_request_t* req, const struct stun_message_t* resp);
int turn_client_channel_bind_onresponse(struct stun_agent_t* stun, struct stun_request_t* req, const struct stun_message_t* resp);

/// turn server recv data from peer, forward data to client
int turn_server_relay(struct stun_agent_t* turn, const struct turn_allocation_t* allocate, const struct sockaddr* peer, const void* data, int bytes);
/// SEND indication: from client to turn server
int turn_server_onsend(struct stun_agent_t* turn, const struct stun_request_t* req);
/// ChannelData: from client to turn server
int turn_server_onchannel_data(struct stun_agent_t* turn, struct turn_allocation_t* allocate, const uint8_t* data, int bytes);
/// DATA indication: from turn server to client
int turn_client_ondata(struct stun_agent_t* turn, const struct stun_request_t* req);
/// ChannelData: from turn server to client
int turn_client_onchannel_data(struct stun_agent_t* turn, struct turn_allocation_t* allocate, const uint8_t* data, int bytes);

struct turn_allocation_t* turn_allocation_create(void);
int turn_allocation_destroy(struct turn_allocation_t** pp);
const struct turn_permission_t* turn_allocation_find_permission(const struct turn_allocation_t* allocate, const struct sockaddr* addr);
int turn_allocation_add_permission(struct turn_allocation_t* allocate, const struct sockaddr* addr);
const struct turn_channel_t* turn_allocation_find_channel(const struct turn_allocation_t* allocate, uint16_t channel);
const struct turn_channel_t* turn_allocation_find_channel_by_peer(const struct turn_allocation_t* allocate, const struct sockaddr* addr);
int turn_allocation_add_channel(struct turn_allocation_t* allocate, const struct sockaddr* addr, uint16_t channel);

struct turn_allocation_t* turn_agent_allocation_find_by_token(struct list_head* root, const void* token);
struct turn_allocation_t* turn_agent_allocation_find_by_relay(struct list_head* root, const struct sockaddr* relayed);
struct turn_allocation_t* turn_agent_allocation_find_by_address(struct list_head* root, const struct sockaddr* host, const struct sockaddr* peer);
int turn_agent_allocation_insert(struct list_head* root, struct turn_allocation_t* allocate);
int turn_agent_allocation_remove(struct list_head* root, struct turn_allocation_t* allocate);
struct turn_allocation_t* turn_agent_allocation_reservation_token(struct stun_agent_t* turn, struct turn_allocation_t* from);

int turn_agent_allocation_cleanup(struct stun_agent_t* turn);

#endif /* _turn_internal_h_ */
