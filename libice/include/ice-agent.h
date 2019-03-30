#ifndef _ice_agent_h_
#define _ice_agent_h_

#if defined(__cplusplus)
extern "C" {
#endif

struct ice_agent_t;
struct ice_agent_t* ice_agent_create();
int ice_agent_destroy(struct ice_agent_t* ice);

/// Add host candidate (with stun/turn server)
/// @param[in] c host candidate (should set stun/turn server addr)
int ice_add_local_candidate(struct ice_agent_t* ice, int stream, const struct ice_candidate_t* c);

/// Add remote candidate(host/server reflexive/relayed)
/// @param[in] c remote candidate
int ice_add_remote_candidate(struct ice_agent_t* ice, int stream, const struct ice_candidate_t* c);

/// Gather server reflexive and relayed candidates
int ice_gather_stun_candidate(struct ice_agent_t* ice);

/// Choosing default candidates
int ice_get_default_candidate(struct ice_agent_t* ice, int stream, struct ice_candidate_t* c);

typedef int(*ice_oncandidate)(void* param, int stream, const struct ice_candidate_t* c);
int ice_list_local_candidate(struct ice_agent_t* ice, int stream, ice_oncandidate oncand, void* param);
int ice_list_remote_candidate(struct ice_agent_t* ice, int stream, ice_oncandidate oncand, void* param);

int ice_start(struct ice_agent_t* ice);

#if defined(__cplusplus)
}
#endif
#endif /* !_ice_agent_h_ */
