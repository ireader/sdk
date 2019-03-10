#ifndef _ice_internal_h_
#define _ice_internal_h_

#include <stdint.h>
#include "sockutil.h"
#include "md5.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

typedef uint16_t ice_component_t; // [1, 256]
typedef uint32_t ice_priority_t;  // [1, 2**31 - 1]
typedef uint8_t ice_foundation_t[32];

enum ice_checklist_state_t
{
	ICE_CHECKLIST_RUNNING = 1,
	ICE_CHECKLIST_COMPLETED,
	ICE_CHECKLIST_FAILED,
};

enum ice_candidate_pair_state_t
{
	ICE_CANDIDATE_PAIR_FROZEN,
	ICE_CANDIDATE_PAIR_WAITING,
	ICE_CANDIDATE_PAIR_INPROGRESS,
	ICE_CANDIDATE_PAIR_SUCCEEDED,
	ICE_CANDIDATE_PAIR_FAILED,
};

enum ice_candidate_type_t
{
	ICE_CANDIDATE_RELAYED = 0, // or VPN
	ICE_CANDIDATE_SERVER_REFLEXIVE = 100,
	ICE_CANDIDATE_PEER_REFLEXIVE = 110,
	ICE_CANDIDATE_HOST = 126,
};

// rounded-time is the current time modulo 20 minutes
// USERNAME = <prefix,rounded-time,clientIP,hmac>
// password = <hmac(USERNAME,anotherprivatekey)>

struct ice_candidate_t
{
	enum ice_candidate_type_t type;
	struct sockaddr_storage addr;
	struct sockaddr_storage base;
	struct sockaddr_storage stun; // stun/turn server
	ice_component_t componentId; // base from 1

	ice_priority_t priority;
	ice_foundation_t foundation;
};

struct ice_candidate_pair_t
{
	struct ice_candidate_t local;
	struct ice_candidate_t remote;
	enum ice_candidate_pair_state_t state;
	int default;
	int valid;
	int nominated;
	int controlling;

	uint64_t priority;
};

// RFC5245 4.1.2. Prioritizing Candidates (p23)
// priority = (2^24)*(type preference) + (2^8)*(local preference) + (2^0)*(256 - component ID)
inline void ice_candidate_priority(struct ice_candidate_t* c)
{
	uint16_t v;
	assert(c->type >= 0 && c->type <= 126);
	assert(c->componentId > 0 && c->componentId <= 256);

	// 1. multihomed and has multiple IP addresses, the local preference for host
	// candidates from a VPN interface SHOULD have a priority of 0.
	// 2. IPv6 > 6to4 > IPv4
	v = (1 << 10) * c->addr.ss_family;

	c->priority = (1 << 24) * c->type + (1 << 8) * v + (256 - c->componentId);
}

// RFC5245 4.1.1.3. Computing Foundations (p22)
inline void ice_candidate_foundation(struct ice_candidate_t* c)
{
	MD5_CTX ctx;
	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)&c->type, sizeof(c->type));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)&c->base, sizeof(c->base));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)&c->stun, sizeof(c->stun));
	MD5Final(c->foundation, &ctx);
}

// RFC5245 5.7.2. Computing Pair Priority and Ordering Pairs
// pair priority = 2^32*MIN(G,D) + 2*MAX(G,D) + (G>D?1:0)
inline void ice_candidate_pair_priority(struct ice_candidate_pair_t* pair)
{
	ice_priority_t G, D;
	G = pair->controlling ? pair->local.priority : pair->remote.priority;
	D = pair->controlling ? pair->remote.priority : pair->local.priority;
	pair->priority = ((uint64_t)1 << 32) * MIN(G, D) + 2 * MAX(G, D) + (G > D ? 1 : 0);
}

#endif /* !_ice_internal_h_ */
