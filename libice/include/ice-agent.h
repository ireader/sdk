#ifndef _ice_agent_h_
#define _ice_agent_h_

#include "ice-candidate.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct ice_agent_handler_t
{
	/// @return 0-ok, other-error
	int (*send)(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes);

	/// turn callback
	void (*ondata)(void* param, const void* data, int bytes, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay);
};

struct ice_agent_t;

/// Create an ice agent
/// @param[in] controlling 1-ice controlling, other-controlled
struct ice_agent_t* ice_create(int controlling, struct ice_agent_handler_t* handler, void* param);
int ice_destroy(struct ice_agent_t* ice);

int ice_set_local_auth(struct ice_agent_t* ice, const char* usr, const char* pwd);
int ice_set_remote_auth(struct ice_agent_t* ice, const char* usr, const char* pwd);

int ice_input(struct ice_agent_t* ice, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, const void* data, int bytes);

/// Add host candidate
/// @param[in] stream audio/video stream id, base 0
/// @param[in] cand local candidate, priority can be 0, all other value must be set
int ice_add_local_candidate(struct ice_agent_t* ice, const struct ice_candidate_t* cand);

/// Add remote candidate(host/server reflexive/relayed)
/// @param[in] stream audio/video stream id, base 0
/// @param[in] c remote candidate
int ice_add_remote_candidate(struct ice_agent_t* ice, const struct ice_candidate_t* cand);

/// Gather server reflexive and relayed candidates
typedef int (*ice_agent_ongather)(void* param, int code);
/// @param[in] addr stun/turn server address
/// @param[in] turn 0-stun server, 1-turn server
int ice_gather_candidate(struct ice_agent_t* ice, const struct sockaddr* addr, int turn, ice_agent_ongather ongather, void* param);

/// Choosing default candidates
int ice_get_default_candidate(struct ice_agent_t* ice, uint8_t stream, uint16_t component, struct ice_candidate_t* cand);

/// @return 0-continue, other-abort
typedef int (*ice_agent_oncandidate)(const struct ice_candidate_t* c, const void* param);
int ice_list_local_candidate(struct ice_agent_t* ice, ice_agent_oncandidate oncand, void* param);
int ice_list_remote_candidate(struct ice_agent_t* ice, ice_agent_oncandidate oncand, void* param);

int ice_start(struct ice_agent_t* ice);
int ice_stop(struct ice_agent_t* ice);

#if defined(__cplusplus)
}
#endif
#endif /* !_ice_agent_h_ */
