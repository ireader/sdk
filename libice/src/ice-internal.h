#ifndef _ice_internal_h_
#define _ice_internal_h_

#include "ice-agent.h"
#include "stun-internal.h"
#include "sys/atomic.h"
#include "sockutil.h"
#include "darray.h"
#include <stdint.h>
#include <assert.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define ICE_TIMER_INTERVAL	20
#define ICE_BINDING_TIMEOUT	200

// agent MUST limit the total number of connectivity checks the agent 
// performs across all check lists to a specific value.
// A default of 100 is RECOMMENDED.
#define ICE_CANDIDATE_LIMIT 10

#define ICE_ROLE_CONFLICT 487

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

typedef struct darray_t ice_candidates_t;
typedef struct darray_t ice_candidate_pairs_t;

struct ice_agent_t
{
	int32_t ref;
	locker_t locker;

	void* timer; // bind/refresh timer
	stun_agent_t* stun;
	ice_candidates_t locals;
	ice_candidates_t remotes;
	ice_candidate_pairs_t valids; // valid list
	enum ice_nomination_t nomination;
	uint64_t tiebreaking; // role conflicts(network byte-order)
	uint8_t stream; // default stream
	int controlling;

	struct ice_checklist_t* list[256];
	struct stun_credential_t auth; // local auth
	struct ice_agent_handler_t handler;
	void* param;
};

// RFC5245 5.7.2. Computing Pair Priority and Ordering Pairs
// pair priority = 2^32*MIN(G,D) + 2*MAX(G,D) + (G>D?1:0)
static inline void ice_candidate_pair_priority(struct ice_candidate_pair_t* pair)
{
	uint32_t G, D;
	G = pair->controlling ? pair->local.priority : pair->remote.priority;
	D = pair->controlling ? pair->remote.priority : pair->local.priority;
	pair->priority = ((uint64_t)1 << 32) * MIN(G, D) + 2 * MAX(G, D) + (G > D ? 1 : 0);
}

struct ice_checklist_t* ice_agent_checklist_create(struct ice_agent_t* ice, int stream);

int ice_agent_init(struct ice_agent_t* ice);
int ice_agent_addref(struct ice_agent_t* ice);
int ice_agent_release(struct ice_agent_t* ice);

int ice_agent_bind(struct ice_agent_t* ice, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, stun_request_handler handler, void* param);
int ice_agent_allocate(struct ice_agent_t* ice, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, stun_request_handler handler, void* param);
int ice_agent_refresh(struct ice_agent_t* ice, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, stun_request_handler handler, void* param);
int ice_agent_connect(struct ice_agent_t* ice, const struct ice_candidate_pair_t* pr, int nominated, stun_request_handler handler, void* param);

#endif /* !_ice_internal_h_ */
