#ifndef _ice_checklist_h_
#define _ice_checklist_h_

#include "ice-agent.h"
#include "ice-internal.h"

struct ice_checklist_t;
struct ice_checklist_handler_t
{
	int (*onrolechanged)(void* param, struct ice_checklist_t* l, int controlling);
	int (*onvalidpair)(void* param, struct ice_checklist_t* l, const struct ice_candidate_pair_t* pair);
	int (*onfinish)(void* param, struct ice_checklist_t* l, int status);
};

struct ice_checklist_t* ice_checklist_create(struct ice_agent_t* ice, struct ice_checklist_handler_t* handler, void* param);
int ice_checklist_destroy(struct ice_checklist_t** l);

int ice_checklist_build(struct ice_checklist_t* l, int stream, const ice_candidates_t* locals, const ice_candidates_t* remotes);

int ice_checklist_init(struct ice_checklist_t* l);

int ice_checklist_update(struct ice_checklist_t* l, const ice_candidate_pairs_t* valids);

int ice_checklist_trigger(struct ice_checklist_t* l, const struct stun_address_t* addr, int nominated);

int ice_checklist_conclude(struct ice_checklist_t* l);

int ice_checklist_stream_valid(struct ice_checklist_t* l, const ice_candidate_pairs_t* valids);

#endif /* !_ice_checklist_h_ */
