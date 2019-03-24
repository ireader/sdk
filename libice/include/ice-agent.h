#ifndef _ice_agent_h_
#define _ice_agent_h_

#if defined(__cplusplus)
extern "C" {
#endif

struct ice_agent_t;
struct ice_agent_t* ice_agent_create();
int ice_agent_destroy(struct ice_agent_t* ice);

int ice_add_local_candidate(struct ice_agent_t* ice, const struct ice_candidate_t* c);
int ice_add_remote_candidate(struct ice_agent_t* ice, const struct ice_candidate_t* c);


#if defined(__cplusplus)
}
#endif
#endif /* !_ice_agent_h_ */
