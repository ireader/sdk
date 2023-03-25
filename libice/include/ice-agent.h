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
	void (*ondata)(void* param, uint8_t stream, uint16_t component, const void* data, int bytes);

	/// Gather server reflexive and relayed candidates
	void (*ongather)(void* param, int code);

	/// ICE nominated
	/// @param[in] flags connected stream bitmask flags, base 0, from Least Significant Bit(LSB), 1-connected, 0-failed
	/// @param[in] mask all streams, base 0, from Least Significant Bit(LSB), 1-connected, 0-failed
	void (*onconnected)(void* param, uint64_t flags, uint64_t mask);
};

struct ice_agent_t;

/// Create an ice agent
/// @param[in] controlling 1-ice controlling, other-controlled
struct ice_agent_t* ice_agent_create(int controlling, struct ice_agent_handler_t* handler, void* param);
int ice_agent_destroy(struct ice_agent_t* ice);

int ice_agent_set_local_auth(struct ice_agent_t* ice, const char* usr, const char* pwd);
int ice_agent_set_remote_auth(struct ice_agent_t* ice, int stream, const char* usr, const char* pwd);

/// Add host candidate
/// @param[in] cand local candidate, priority can be 0, all other value must be set
int ice_agent_add_local_candidate(struct ice_agent_t* ice, const struct ice_candidate_t* cand);

/// Add remote candidate(host/server reflexive/relayed)
/// @param[in] cand remote candidate
int ice_agent_add_remote_candidate(struct ice_agent_t* ice, const struct ice_candidate_t* cand);

/// @return 0-continue, other-abort
typedef int(*ice_agent_oncandidate)(const struct ice_candidate_t* c, void* param);
int ice_agent_list_local_candidate(struct ice_agent_t* ice, ice_agent_oncandidate oncand, void* param);
int ice_agent_list_remote_candidate(struct ice_agent_t* ice, ice_agent_oncandidate oncand, void* param);

/// Gather reflexive/relayed candidates
/// @param[in] addr stun/turn server address
/// @param[in] turn 0-stun server, 1-turn server
int ice_agent_gather(struct ice_agent_t* ice, const struct sockaddr* addr, int turn, int timeout, int credential, const char* usr, const char* pwd);

/// 1. before ice connected: get default candidate
/// 2. after ice connected: get nominated candidate
int ice_agent_get_local_candidate(struct ice_agent_t* ice, uint8_t stream, uint16_t component, struct ice_candidate_t* cand);
int ice_agent_get_remote_candidate(struct ice_agent_t* ice, uint8_t stream, uint16_t component, struct ice_candidate_t* cand);

int ice_agent_start(struct ice_agent_t* ice);
int ice_agent_stop(struct ice_agent_t* ice);

/// @param[in] enable 1-enable ice-lite, 0-disable ice-lite
int ice_agent_set_icelite(struct ice_agent_t* ice, int enable);

int ice_agent_input(struct ice_agent_t* ice, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes);

int ice_agent_send(struct ice_agent_t* ice, uint8_t stream, uint16_t component, const void* data, int bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_ice_agent_h_ */
