#include "ice-agent.h"
#include "ice-internal.h"
#include "stun-agent.h"
#include "stun-internal.h"
#include "ice-checklist.h"
#include "ice-candidates.h"

static void ice_agent_clear_checklist(struct ice_agent_t* ice);

struct ice_agent_t* ice_create(int controlling, struct ice_agent_handler_t* handler, void* param)
{
	struct ice_agent_t* ice;
	ice = (struct ice_agent_t*)calloc(1, sizeof(struct ice_agent_t));
	if (ice)
	{
		locker_create(&ice->locker);
		ice->ref = 1;
		ice->param = param;
		ice->nomination = ICE_REGULAR_NOMINATION;
		ice->controlling = controlling;
		ice->tiebreaking = (intptr_t)ice * rand();
		memcpy(&ice->handler, handler, sizeof(ice->handler));
		ice_candidate_pairs_init(&ice->valids);
		ice_agent_init(ice);
	}

	return ice;
}

int ice_destroy(struct ice_agent_t* ice)
{
	return ice_agent_release(ice);
}

int ice_agent_addref(struct ice_agent_t* ice)
{
	assert(ice->ref > 0);
	return atomic_increment32(&ice->ref);
}

int ice_agent_release(struct ice_agent_t* ice)
{
	int ref;
	ref = atomic_decrement32(&ice->ref);
	assert(ref >= 0);
	if (0 != ref)
		return ref;

	if(ice->timer)
		stun_timer_stop(ice->timer);
	ice_agent_clear_checklist(ice);
	ice_candidate_pairs_free(&ice->valids);
	ice_candidates_free(&ice->remotes);
	ice_candidates_free(&ice->locals);
	stun_agent_destroy(&ice->stun);
	locker_destroy(&ice->locker);
	free(ice);
	return 0;
}

int ice_input(struct ice_agent_t* ice, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, const void* data, int bytes)
{
	return ice && ice->stun ? stun_agent_input(ice->stun, protocol, local, remote, relay, data, bytes) : -1;
}

int ice_set_local_auth(struct ice_agent_t* ice, const char* usr, const char* pwd)
{
	memset(&ice->auth, 0, sizeof(ice->auth));
	ice->auth.credential = STUN_CREDENTIAL_SHORT_TERM;
	snprintf(ice->auth.usr, sizeof(ice->auth.usr) , "%s", usr ? usr : "");
	snprintf(ice->auth.pwd, sizeof(ice->auth.pwd), "%s", pwd ? pwd : "");
	return 0;
}

int ice_set_remote_auth(struct ice_agent_t* ice, const char* usr, const char* pwd)
{
	memset(&ice->rauth, 0, sizeof(ice->rauth));
	ice->rauth.credential = STUN_CREDENTIAL_SHORT_TERM;
	snprintf(ice->rauth.usr, sizeof(ice->rauth.usr), "%s", usr ? usr : "");
	snprintf(ice->rauth.pwd, sizeof(ice->rauth.pwd), "%s", pwd ? pwd : "");
	return 0;
}

int ice_add_local_candidate(struct ice_agent_t* ice, const struct ice_candidate_t* cand)
{
	if ( !cand || 0 == cand->priority || 0 == cand->foundation[0] || cand->component < 1 || cand->component > 256
		||(ICE_CANDIDATE_HOST != cand->type && ICE_CANDIDATE_SERVER_REFLEXIVE != cand->type && ICE_CANDIDATE_RELAYED != cand->type && ICE_CANDIDATE_PEER_REFLEXIVE != cand->type)
		|| (STUN_PROTOCOL_UDP != cand->protocol && STUN_PROTOCOL_TCP != cand->protocol && STUN_PROTOCOL_TLS != cand->protocol && STUN_PROTOCOL_DTLS != cand->protocol)
		//|| (AF_INET != cand->reflexive.ss_family && AF_INET6 != cand->reflexive.ss_family)
		//|| (AF_INET != cand->relay.ss_family && AF_INET6 != cand->relay.ss_family)
		|| (AF_INET != cand->host.ss_family && AF_INET6 != cand->host.ss_family))
	{
		assert(0);
		return -1;
	}

	if (ice_candidates_count(&ice->locals) > ICE_CANDIDATE_LIMIT)
		return -1;
	return ice_candidates_insert(&ice->locals, cand);
}

int ice_add_remote_candidate(struct ice_agent_t* ice, const struct ice_candidate_t* cand)
{
	if (!cand || 0 == cand->priority || 0 == cand->foundation[0] || cand->component < 1 || cand->component > 256
		|| (ICE_CANDIDATE_HOST != cand->type && ICE_CANDIDATE_SERVER_REFLEXIVE != cand->type && ICE_CANDIDATE_RELAYED != cand->type && ICE_CANDIDATE_PEER_REFLEXIVE != cand->type)
		|| (STUN_PROTOCOL_UDP != cand->protocol && STUN_PROTOCOL_TCP != cand->protocol && STUN_PROTOCOL_TLS != cand->protocol && STUN_PROTOCOL_DTLS != cand->protocol)
		|| NULL == ice_candidate_addr(cand) || (AF_INET != ice_candidate_addr(cand)->ss_family && AF_INET6 != ice_candidate_addr(cand)->ss_family))
	{
		assert(0);
		return -1;
	}

	if (ice_candidates_count(&ice->remotes) > ICE_CANDIDATE_LIMIT)
		return -1;
	return ice_candidates_insert(&ice->remotes, cand);
}

int ice_list_local_candidate(struct ice_agent_t* ice, ice_agent_oncandidate oncand, void* param)
{
	return ice_candidates_list(&ice->locals, oncand, param);
}

int ice_list_remote_candidate(struct ice_agent_t* ice, ice_agent_oncandidate oncand, void* param)
{
	return ice_candidates_list(&ice->remotes, oncand, param);
}

int ice_get_default_candidate(struct ice_agent_t* ice, uint8_t stream, uint16_t component, struct ice_candidate_t* c)
{
	int i;
	struct ice_candidate_t *p, *pc = NULL;
	for (i = 0; i < ice_candidates_count(&ice->locals); i++)
	{
		p = ice_candidates_get(&ice->locals, i);
		if (p->stream != stream || p->component != component)
			continue;

		// rfc5245 4.1.4. Choosing Default Candidates (p25)
		// 1. It is RECOMMENDED that default candidates be chosen based on the likelihood 
		//    of those candidates to work with the peer that is being contacted. 
		// 2. It is RECOMMENDED that the default candidates are the relayed candidates 
		//    (if relayed candidates are available), server reflexive candidates 
		//    (if server reflexive candidates are available), and finally host candidates.
		if (NULL == pc || pc->priority > p->priority)
			pc = p;
	}

	if (NULL == pc || NULL == c)
		return -1; // not found local candidate

	memcpy(c, pc, sizeof(struct ice_candidate_t));
	return 0;
}

static void ice_agent_clear_checklist(struct ice_agent_t* ice)
{
	int i;
	for (i = 0; i < sizeof(ice->list) / sizeof(ice->list[0]); i++)
	{
		if (ice->list[i])
			ice_checklist_destroy(&ice->list[i]);
	}
}

int ice_start(struct ice_agent_t* ice)
{
	int i, r;
	struct ice_checklist_t* active;
	struct ice_checklist_t* check;
	struct ice_candidate_t *local;

	active = NULL; // default stream
	ice_agent_clear_checklist(ice);
	ice_candidate_pairs_clear(&ice->valids);

	assert(ice_candidates_count(&ice->locals) > 0);
	for (i = 0; i < ice_candidates_count(&ice->locals); i++)
	{
		local = ice_candidates_get(&ice->locals, i);		
		check = ice_agent_checklist_create(ice, local->stream);
		assert(!ice->list[local->stream]);
		ice->list[local->stream] = check;
		if (NULL == active || local->stream == ice->stream)
			active = check;
	}

	if (!active)
		return -1;

	r = ice_checklist_start(active);
	if (0 != r)
		ice_agent_clear_checklist(ice);	
	return r;
}

int ice_stop(struct ice_agent_t* ice)
{
	// TODO
	assert(0);
	return -1;
}
