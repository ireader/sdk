#ifndef _ice_candidate_h_
#define _ice_candidate_h_

#include <stdint.h>

enum ice_candidate_type_t
{
	ICE_CANDIDATE_HOST = 0,
	ICE_CANDIDATE_SERVER_REFLEXIVE,
	ICE_CANDIDATE_PEER_REFLEXIVE,
	ICE_CANDIDATE_RELAYED, // or VPN

	ICE_CANDIDATE_UNKNOWN,
};

struct ice_candidate_t
{
	enum ice_candidate_type_t type;	// candidate type, e.g. ICE_CANDIDATE_HOST
	struct sockaddr_storage addr; // HOST: host address, REFLEXIVE: stun/turn reflexive address, RELAYED: relayed address
	struct sockaddr_storage host; // host address(local candidate only)

	uint8_t stream;
	uint16_t component; // rtp/rtcp component id, [1, 256]
	uint32_t priority; // [1, 2**31 - 1]
	char foundation[33];
	int protocol; // stun/turn protocol, e.g. STUN_PROTOCOL_UDP
};

static inline const struct sockaddr_storage* ice_candidate_base(const struct ice_candidate_t* c)
{
	// The base of a server reflexive candidate is the host candidate
	// from which it was derived. A host candidate is also said to have
	// a base, equal to that candidate itself. Similarly, the base of a
	// relayed candidate is that candidate itself.
	assert(ICE_CANDIDATE_HOST == c->type || ICE_CANDIDATE_RELAYED == c->type || ICE_CANDIDATE_SERVER_REFLEXIVE == c->type || ICE_CANDIDATE_PEER_REFLEXIVE == c->type);
	return ICE_CANDIDATE_RELAYED != c->type ? &c->host : &c->addr;
}

/// ice candidate rel-addr/rel-port
static inline const struct sockaddr_storage* ice_candidate_realaddr(const struct ice_candidate_t* c)
{
	assert(ICE_CANDIDATE_HOST == c->type || ICE_CANDIDATE_RELAYED == c->type || ICE_CANDIDATE_SERVER_REFLEXIVE == c->type || ICE_CANDIDATE_PEER_REFLEXIVE == c->type);
	return ICE_CANDIDATE_RELAYED == c->type ? &c->host /*&c->reflexive*/ : &c->host;
}

static inline const char* ice_candidate_typename(const struct ice_candidate_t* c)
{
	static const char* s_name[] = { "host", "srflx", "prflx", "relay", };
	assert(c->type >= ICE_CANDIDATE_HOST && c->type <= ICE_CANDIDATE_RELAYED);
	return s_name[(c->type - ICE_CANDIDATE_HOST) % 4];
}

// RFC5245 4.1.2. Prioritizing Candidates (p23)
// priority = (2^24)*(type preference) + (2^8)*(local preference) + (2^0)*(256 - component ID)
static inline void ice_candidate_priority(struct ice_candidate_t* c)
{
	uint16_t v;
	static const uint8_t s_priority[] = { 126 /*host*/, 100 /*srflx*/, 110 /*prflx*/, 0 /*relay*/ };
	assert(c->type >= ICE_CANDIDATE_HOST && c->type <= ICE_CANDIDATE_RELAYED);
	assert(c->component > 0 && c->component <= 256);

	// 1. multi-homed and has multiple IP addresses, the local preference for host
	// candidates from a VPN interface SHOULD have a priority of 0.
	// 2. IPv6 > 6to4 > IPv4
	v = (1 << 10) * c->host.ss_family;

	c->priority = (1 << 24) * s_priority[(c->type - ICE_CANDIDATE_HOST) % 4] + (1 << 8) * v + (256 - c->component);
}

#endif /* !_ice_candidate_h_ */
