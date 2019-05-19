#ifndef _ice_agent_h_
#define _ice_agent_h_

#include "sys/sock.h"
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum ice_candidate_type_t
{
	ICE_CANDIDATE_RELAYED = 0, // or VPN
	ICE_CANDIDATE_SERVER_REFLEXIVE = 100,
	ICE_CANDIDATE_PEER_REFLEXIVE = 110,
	ICE_CANDIDATE_HOST = 126,
};

// ice candidate rel-addr/rel-port
#define ICE_CANDIDATE_RELADDR(c) (ICE_CANDIDATE_RELAYED == (c)->type ? &(c)->reflexive : &(c)->host)

struct ice_candidate_t
{
	enum ice_candidate_type_t type; // candidate type, e.g. ICE_CANDIDATE_HOST
	struct sockaddr_storage addr; // ICE_CANDIDATE_RELAYED: relayed address, other: server reflexive address(NAT/Router address)
	struct sockaddr_storage host; // local host address
	struct sockaddr_storage reflexive; // stun/turn reflexive address

	uint16_t component; // rtp/rtcp component id, [1, 256]
	uint32_t priority; // [1, 2**31 - 1]
	char foundation[33];
	int protocol; // stun/turn protocol, e.g. STUN_PROTOCOL_UDP
};

struct ice_agent_handler_t
{
	/// @return 0-ok, other-error
	int (*send)(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes);

	/// @param[out] pwd password of the usr
	/// @return 0-ok, other-error
	int (*auth)(void* param, const char* usr, char pwd[256]);

	/// turn callback
	void (*ondata)(void* param, const void* data, int bytes, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay);
};

struct ice_agent_t;
struct ice_agent_t* ice_create(struct ice_agent_handler_t* handler, void* param);
int ice_destroy(struct ice_agent_t* ice);

int ice_set_local_auth(struct ice_agent_t* ice, const char* usr, const char* pwd);

int ice_input(struct ice_agent_t* ice, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, const void* data, int bytes);

/// Add host candidate
/// @param[in] stream audio/video stream id, base 0
/// @param[in] cand local candidate, priority can be 0, all other value must be set
int ice_add_local_candidate(struct ice_agent_t* ice, int stream, const struct ice_candidate_t* cand);

/// Add remote candidate(host/server reflexive/relayed)
/// @param[in] stream audio/video stream id, base 0
/// @param[in] c remote candidate
int ice_add_remote_candidate(struct ice_agent_t* ice, int stream, const struct ice_candidate_t* cand);

/// Gather server reflexive and relayed candidates
typedef int (*ice_agent_ongather)(void* param, int code);
/// @param[in] addr stun/turn server address
/// @param[in] turn 0-stun server, 1-turn server
int ice_gather_stun_candidate(struct ice_agent_t* ice, const struct sockaddr* addr, int turn, ice_agent_ongather ongather, void* param);

/// Choosing default candidates
int ice_get_default_candidate(struct ice_agent_t* ice, int stream, uint16_t component, struct ice_candidate_t* cand);

typedef int (*ice_agent_oncandidate)(void* param, const struct ice_candidate_t* c);
int ice_list_local_candidate(struct ice_agent_t* ice, int stream, ice_agent_oncandidate oncand, void* param);
int ice_list_remote_candidate(struct ice_agent_t* ice, int stream, ice_agent_oncandidate oncand, void* param);

int ice_start(struct ice_agent_t* ice);

#if defined(__cplusplus)
}
#endif
#endif /* !_ice_agent_h_ */
