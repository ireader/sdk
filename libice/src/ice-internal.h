#ifndef _ice_internal_h_
#define _ice_internal_h_

#include "ice-agent.h"
#include <stdint.h>
#include "sockutil.h"
#include "md5.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define ICE_TIMER_INTERVAL	20
#define ICE_BINDING_TIMEOUT	200

// agent MUST limit the total number of connectivity checks the agent 
// performs across all check lists to a specific value.
// A default of 100 is RECOMMENDED.
#define ICE_CANDIDATE_LIMIT 10

enum ice_checklist_state_t
{
	ICE_CHECKLIST_FROZEN = 0,
	ICE_CHECKLIST_RUNNING,
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

enum ice_nomination_t
{
	ICE_REGULAR_NOMINATION = 0,
	ICE_AGGRESSIVE_NOMINATION,
};

// rounded-time is the current time modulo 20 minutes
// USERNAME = <prefix,rounded-time,clientIP,hmac>
// password = <hmac(USERNAME,anotherprivatekey)>

struct ice_candidate_pair_t
{
	struct ice_candidate_t local;
	struct ice_candidate_t remote;
	enum ice_candidate_pair_state_t state;
	int valid;
	int nominated;
	int controlling;

	uint64_t priority;
	char foundation[66];
};

// RFC5245 4.1.2. Prioritizing Candidates (p23)
// priority = (2^24)*(type preference) + (2^8)*(local preference) + (2^0)*(256 - component ID)
static inline void ice_candidate_priority(struct ice_candidate_t* c)
{
	uint16_t v;
	assert(c->type >= 0 && c->type <= 126);
	assert(c->component > 0 && c->component <= 256);

	// 1. multihomed and has multiple IP addresses, the local preference for host
	// candidates from a VPN interface SHOULD have a priority of 0.
	// 2. IPv6 > 6to4 > IPv4
	v = (1 << 10) * c->addr.ss_family;

	c->priority = (1 << 24) * c->type + (1 << 8) * v + (256 - c->component);
}

// RFC5245 4.1.1.3. Computing Foundations (p22)
static inline void ice_candidate_foundation(struct ice_candidate_t* c, const struct sockaddr* stun)
{
	// 1. they are of the same type (host, relayed, server reflexive, or peer reflexive).
	// 2. their bases have the same IP address (the ports can be different).
	// 3. for reflexive and relayed candidates, the STUN or TURN servers used to obtain them have the same IP address.
	// 4. they were obtained using the same transport protocol (TCP, UDP, etc.).

	int i;
	MD5_CTX ctx;
	unsigned char md5[16];
	static const char* s_base16_enc = "0123456789ABCDEF";

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)&c->type, sizeof(c->type));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)&c->protocol, sizeof(c->protocol));
	MD5Update(&ctx, (unsigned char*)":", 1);
	
	MD5Update(&ctx, (unsigned char*)&c->raddr.ss_family, sizeof(c->raddr.ss_family));
	if (AF_INET == c->raddr.ss_family)
		MD5Update(&ctx, (unsigned char*)&((struct sockaddr_in*)&c->raddr)->sin_addr, 4);
	else if (AF_INET6 == c->raddr.ss_family)
		MD5Update(&ctx, (unsigned char*)&((struct sockaddr_in6*)&c->raddr)->sin6_addr, 8);

	MD5Update(&ctx, (unsigned char*)":", 1);
	if (ICE_CANDIDATE_HOST != c->type)
		MD5Update(&ctx, (unsigned char*)stun, socket_addr_len(stun));
	else
		MD5Update(&ctx, (unsigned char*)&c->raddr, socket_addr_len((struct sockaddr*)&c->raddr));
	MD5Final(md5, &ctx);

	assert(sizeof(c->foundation) >= 2 * sizeof(md5));
	for (i = 0; i < sizeof(md5) && i < sizeof(c->foundation); i++)
	{
		c->foundation[i * 2] = s_base16_enc[(md5[i] >> 4) & 0x0F];
		c->foundation[i * 2 + 1] = s_base16_enc[md5[i] & 0x0F];
	}
}

// RFC5245 5.7.2. Computing Pair Priority and Ordering Pairs
// pair priority = 2^32*MIN(G,D) + 2*MAX(G,D) + (G>D?1:0)
static inline void ice_candidate_pair_priority(struct ice_candidate_pair_t* pair)
{
	uint32_t G, D;
	G = pair->controlling ? pair->local.priority : pair->remote.priority;
	D = pair->controlling ? pair->remote.priority : pair->local.priority;
	pair->priority = ((uint64_t)1 << 32) * MIN(G, D) + 2 * MAX(G, D) + (G > D ? 1 : 0);
}

#endif /* !_ice_internal_h_ */
