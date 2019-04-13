#ifndef _ice_checklist_h_
#define _ice_checklist_h_

#include "ice-agent.h"
#include "ice-internal.h"
#include "ice-candidates.h"
#include "stun-internal.h"

struct ice_checklist_t;

struct ice_checklist_handler_t
{
	int (*onvalid)(void* param, struct ice_checklist_t* l, const struct ice_candidate_pair_t* pair);
	int (*onfinish)(void* param, struct ice_checklist_t* l, int status);
};

struct ice_checklist_t* ice_checklist_create(stun_agent_t* stun, const struct stun_credetial_t* auth, struct ice_checklist_handler_t* handler, void* param);
int ice_checklist_destroy(struct ice_checklist_t** l);

int ice_checklist_add_local_candidate(struct ice_checklist_t* l, const struct ice_candidate_t* c);

/// Add remote candidate(host/server reflexive/relayed)
/// @param[in] c remote candidate
int ice_checklist_add_remote_candidate(struct ice_checklist_t* l, const struct ice_candidate_t* c);

int ice_checklist_list_local_candidate(struct ice_checklist_t* l, ice_agent_oncandidate oncand, void* param);
int ice_checklist_list_remote_candidate(struct ice_checklist_t* l, ice_agent_oncandidate oncand, void* param);

/// Choosing default candidates
int ice_checklist_get_default_candidate(struct ice_checklist_t* l, ice_component_t component, struct ice_candidate_t* c);

int ice_checklist_gather_stun_candidate(struct ice_checklist_t* l, ice_agent_ongather ongather, void* param);

int ice_checklist_build(struct ice_checklist_t* l);

int ice_checklist_init(struct ice_checklist_t* l);

int ice_checklist_update(struct ice_checklist_t* l, const ice_candidate_pairs_t* valids);

int ice_checklist_trigger(struct ice_checklist_t* l, int protocol, const struct sockaddr_storage* local, const struct sockaddr_storage* remote);

int ice_checklist_stream_valid(struct ice_checklist_t* l, struct darray_t* valids);

#endif /* !_ice_checklist_h_ */
