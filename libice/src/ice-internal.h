#ifndef _ice_internal_h_
#define _ice_internal_h_

#include "stun-internal.h"
#include "ice-agent.h"
#include "sys/atomic.h"
#include "sockutil.h"
#include "darray.h"
#include "list.h"
#include <stdint.h>
#include <assert.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define ICE_STREAM_MAX		8
#define ICE_COMPONENT_MAX	4
#define ICE_TIMER_INTERVAL	20
#define ICE_BINDING_TIMEOUT	200

// agent MUST limit the total number of connectivity checks the agent 
// performs across all check lists to a specific value.
// A default of 100 is RECOMMENDED.
#define ICE_CANDIDATE_LIMIT 32

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

struct ice_stream_t;
struct ice_candidate_pair_t
{
	struct ice_candidate_t local;
	struct ice_candidate_t remote;
	enum ice_candidate_pair_state_t state;
	int nominated; // use-candidate

	uint64_t priority;
	char foundation[66];
	struct ice_stream_t* stream;
};

typedef struct darray_t ice_candidates_t;
typedef struct darray_t ice_candidate_pairs_t;

struct ice_stream_t
{
	struct list_head link;
	int stream; // stream id
	int status;

	ice_candidates_t locals;
	ice_candidates_t remotes;
	struct ice_checklist_t* checklist;
	struct stun_credential_t rauth; // remote auth

	// nominated candidates(for each component)
	struct ice_candidate_pair_t components[ICE_COMPONENT_MAX];
	int ncomponent;
};

struct ice_agent_t
{
	//locker_t locker;

	stun_agent_t* stun;
	struct list_head streams;
	enum ice_nomination_t nomination;
	uint64_t tiebreaking; // role conflicts(network byte-order)
	int controlling;
	int icelite;

	struct sockaddr_storage saddr; // stun/turn server addr
	struct stun_credential_t auth; // local auth
	struct stun_credential_t sauth; // stun/turn auth
	struct ice_agent_handler_t handler;
	void* param;
};

// RFC5245 5.7.2. Computing Pair Priority and Ordering Pairs
// pair priority = 2^32*MIN(G,D) + 2*MAX(G,D) + (G>D?1:0)
static inline void ice_candidate_pair_priority(struct ice_candidate_pair_t* pair, int controlling)
{
	uint32_t G, D;
	G = controlling ? pair->local.priority : pair->remote.priority;
	D = controlling ? pair->remote.priority : pair->local.priority;
	pair->priority = ((uint64_t)1 << 32) * MIN(G, D) + 2 * MAX(G, D) + (G > D ? 1 : 0);
}

static inline void ice_candidate_pair_foundation(struct ice_candidate_pair_t* pair)
{
	snprintf(pair->foundation, sizeof(pair->foundation), "%s\n%s", pair->local.foundation, pair->remote.foundation);
}

/// ICE foundation update
void ice_candidate_foundation(struct ice_agent_t* ice, struct ice_candidate_t* c);

int ice_agent_bind(struct ice_agent_t* ice, const struct stun_credential_t* auth, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, int timeout, stun_request_handler handler, void* param);
int ice_agent_allocate(struct ice_agent_t* ice, const struct stun_credential_t* auth, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, int timeout, stun_request_handler handler, void* param);
int ice_agent_refresh(struct ice_agent_t* ice, const struct stun_credential_t* auth, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, int timeout, stun_request_handler handler, void* param);
int ice_agent_create_permission(struct ice_agent_t* ice, const struct stun_credential_t* auth, const struct sockaddr* local, const struct sockaddr* turn, const struct sockaddr* peer, int timeout, stun_request_handler handler, void* param);
int ice_agent_channel_bind(struct ice_agent_t* ice, const struct stun_credential_t* auth, const struct sockaddr* local, const struct sockaddr* turn, const struct sockaddr* peer, uint16_t channel, int timeout, stun_request_handler handler, void* param);
int ice_agent_connect(struct ice_agent_t* ice, const struct ice_candidate_pair_t* pr, int nominated, int timeout, stun_request_handler handler, void* param);

int ice_stream_destroy(struct ice_stream_t** pp);
int ice_agent_active_checklist_count(struct ice_agent_t* ice);
struct ice_stream_t* ice_agent_find_stream(struct ice_agent_t* ice, int stream);
struct ice_candidate_t* ice_agent_find_local_candidate(struct ice_agent_t* ice, const struct sockaddr_storage* host);
struct ice_candidate_t* ice_agent_find_remote_candidate(struct ice_agent_t* ice, const struct sockaddr_storage* addr);

int ice_agent_onrolechanged(void* param);
int ice_agent_add_peer_reflexive_candidate(struct ice_agent_t* ice, const struct stun_address_t* addr, const struct stun_attr_t* priority);
int ice_agent_add_remote_peer_reflexive_candidate(struct ice_agent_t* ice, uint8_t stream, uint16_t component, const struct stun_address_t* addr, const struct stun_attr_t* priority);

#endif /* !_ice_internal_h_ */
