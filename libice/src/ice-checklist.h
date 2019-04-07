#ifndef _ice_checklist_h_
#define _ice_checklist_h_

#include "ice-internal.h"
#include "ice-agent.h"
#include "stun-agent.h"

struct ice_checklist_t;

struct ice_checklist_t* ice_checklist_create(stun_agent_t* stun);
int ice_checklist_destroy(struct ice_checklist_t** l);

int ice_checklist_add_local_candidate(struct ice_checklist_t* l, const struct ice_candidate_t* c);

/// Add remote candidate(host/server reflexive/relayed)
/// @param[in] c remote candidate
int ice_checklist_add_remote_candidate(struct ice_checklist_t* l, const struct ice_candidate_t* c);

/// Gather server reflexive and relayed candidates
int ice_checklist_gather_stun_candidate(struct ice_checklist_t* l, ice_agent_ongather ongather, void* param);

/// Choosing default candidates
int ice_checklist_get_default_candidate(struct ice_checklist_t* l, ice_component_t component, struct ice_candidate_t* c);

int ice_checklist_list_local_candidate(struct ice_checklist_t* l, ice_agent_oncandidate oncand, void* param);
int ice_checklist_list_remote_candidate(struct ice_checklist_t* l, ice_agent_oncandidate oncand, void* param);

int ice_checklist_onbind(struct ice_checklist_t* l, const struct sockaddr_storage* addr);

int ice_checklist_rebuild(struct ice_checklist_t* l);

int ice_checklist_start(struct ice_checklist_t* l);

#endif /* !_ice_checklist_h_ */
