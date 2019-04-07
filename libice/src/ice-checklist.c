#include "ice-checklist.h"
#include "ice-candidates.h"

struct ice_checklist_t
{
	enum ice_checklist_state_t state;

	stun_agent_t* stun;
	ice_candidates_t locals;
	ice_candidates_t remotes;

	struct darray_t components; // ordinary check, pairs base on component
	struct darray_t trigger; // trigger check list
	struct darray_t valids; // valid list
};

struct ice_checklist_t* ice_checklist_create(stun_agent_t* stun)
{
	struct ice_checklist_t* l;
	l = (struct ice_checklist_t*)calloc(1, sizeof(struct ice_checklist_t));
	if (l)
	{
		l->stun = stun;
		l->state = ICE_CHECKLIST_FROZEN;
		ice_candidates_init(&l->locals);
		ice_candidates_init(&l->remotes);
		darray_init(&l->components, sizeof(struct darray_t), 2);
	}
	return l;
}

int ice_checklist_destroy(struct ice_checklist_t** pl)
{
	struct ice_checklist_t* l;
	if (!pl || !*pl)
		return -1;
	
	l = *pl;
	ice_candidates_free(&l->locals);
	ice_candidates_free(&l->remotes);
	darray_free(&l->components);
	free(l);
	*pl = NULL;
	return 0;
}

int ice_checklist_add_local_candidate(struct ice_checklist_t* l, const struct ice_candidate_t* c)
{
	if (c->type != ICE_CANDIDATE_HOST)
		return -1;
	if (AF_INET != c->addr.ss_family && AF_INET6 != c->addr.ss_family)
		return -1;
	if (c->addr.ss_family != c->stun.ss_family)
		return -1;

	if (0 == c->priority)
		ice_candidate_priority(c);
	if (0 == c->foundation)
		ice_candidate_foundation(c);

	return ice_candidates_insert(&l->locals, c);
}

int ice_checklist_add_remote_candidate(struct ice_checklist_t* l, const struct ice_candidate_t* c)
{
	if (c->type != ICE_CANDIDATE_HOST)
		return -1;
	if (AF_INET != c->addr.ss_family && AF_INET6 != c->addr.ss_family)
		return -1;
	if (c->addr.ss_family != c->stun.ss_family)
		return -1;

	if (0 == c->priority)
		ice_candidate_priority(c);
	if (0 == c->foundation)
		ice_candidate_foundation(c);

	return ice_candidates_insert(&l->remotes, c);
}

static int ice_checklist_ongather(void* param, const stun_transaction_t* resp, int code, const char* phrase)
{
}

int ice_checklist_gather_stun_candidate(struct ice_checklist_t* l, ice_agent_ongather ongather, void* param)
{
	int i;
	struct stun_transaction_t* req;
	struct ice_candidate_t *p, *pc = NULL;
	for (i = 0; i < darray_count(&l->locals); i++)
	{
		p = (struct ice_candidate_t*)darray_get(&l->locals, i);
		if (p->type != ICE_CANDIDATE_HOST)
			continue;

		req = stun_transaction_create(l->stun, STUN_RFC_5389, ice_checklist_ongather, l);
		stun_transaction_setaddr(req, STUN_PROTOCOL_UDP, &p->addr, &p->stun);
		stun_transaction_setauth(req, req->auth.credential, req->auth.usr, req->auth.pwd, req->auth.realm, req->auth.nonce);
		r = stun_agent_bind(req);
	}
}

int ice_checklist_get_default_candidate(struct ice_checklist_t* l, ice_component_t component, struct ice_candidate_t* c)
{
	int i;
	struct ice_candidate_t *p, *pc = NULL;
	for (i = 0; i < darray_count(&l->locals); i++)
	{
		p = (struct ice_candidate_t*)darray_get(&l->locals, i);
		if(p->componentId != component)
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

	if (NULL == pc)
		return -1; // not found local candidate

	memcpy(c, pc, sizeof(struct ice_candidate_t));
	return 0;
}

int ice_checklist_list_local_candidate(struct ice_checklist_t* l, ice_agent_oncandidate oncand, void* param)
{
	int i, r;
	struct ice_candidate_t* c;
	for (i = 0; i < darray_count(&l->locals); i++)
	{
		c = (struct ice_candidate_t*)darray_get(&l->locals, i);
		r = oncand(param, c);
		if (0 != r)
			return r;
	}

	return 0;
}

int ice_checklist_list_remote_candidate(struct ice_checklist_t* l, ice_agent_oncandidate oncand, void* param)
{
	int i, r;
	struct ice_candidate_t* c;
	for (i = 0; i < darray_count(&l->remotes); i++)
	{
		c = (struct ice_candidate_t*)darray_get(&l->remotes, i);
		r = oncand(param, c);
		if (0 != r)
			return r;
	}

	return 0;
}

static struct darray_t* ice_checklist_find_pairs(struct ice_checklist_t* l, ice_component_t component)
{
	int i;
	struct darray_t* arr;
	struct ice_candidate_pair_t* pair;
	for (i = 0; i < darray_count(&l->components); i++)
	{
		arr = (struct darray_t*)darray_get(&l->components, i);
		if(darray_count(arr) < 1)
			continue;

		pair = (struct ice_candidate_pair_t*)darray_get(arr, 0);
		if (pair->local.componentId == component)
			return arr;
	}

	return NULL;
}

int ice_checklist_rebuild(struct ice_checklist_t* l)
{
	int i, j;
	struct darray_t* component, arr;
	struct ice_candidate_t *local, *remote;
	struct ice_candidate_pair_t pair;

	// TODO: free pairs
	l->components.count = 0; // reset pairs size

	for (i = 0; i < ice_candidates_count(&l->locals); i++)
	{
		local = ice_candidates_get(&l->locals, i);

		component = ice_checklist_find_pairs(l, local->componentId);
		if (!component)
		{
			darray_init(&arr, sizeof(struct ice_candidate_pair_t), 9);
			darray_push_back(&l->components, &arr, 1);
			component = (struct darray_t*)darray_get(&l->components, darray_count(&l->components) - 1);
		}

		for (j = 0; j < ice_candidates_count(&l->remotes); j++)
		{
			remote = ice_candidates_get(&l->remotes, i);
			if(local->componentId != remote->componentId)
				continue;

			memset(&pair, 0, sizeof(pair));
			pair.state = ICE_CANDIDATE_PAIR_FROZEN;
			ice_candidate_pair_priority(&pair);
			memcpy(&pair.local, local, sizeof(struct ice_candidate_t));
			memcpy(&pair.remote, remote, sizeof(struct ice_candidate_t));
			ice_candidate_pairs_insert(component, &pair);
		}
	}
}

// When a check list is first constructed as the consequence of an
// offer/answer exchange, it is placed in the Running state.

// rfc5245 5.8. Scheduling Checks (p37)
// 1. When the timer fires, the agent removes the top pair from 
//    the triggered check queue, performs a connectivity check on that
//    pair, and sets the state of the candidate pair to In-Progress.
// 2. If there are no pairs in the triggered check queue, an ordinary check is sent.
// 3. Find the highest-priority pair in that check list that is in the Waiting state.
// 4. Find the highest-priority pair in that check list that is in the Frozen state.
// 5. Terminate the timer for that check list.

// rfc5245 7.1.3.2.3. Updating Pair States (45)
// 1. The agent changes the states for all other Frozen pairs for the
//    same media stream and same foundation to Waiting. Typically, but
//    not always, these other pairs will have different component IDs.
// 2. If there is a pair in the valid list for every component of this
//    media stream, the success of this check may unfreeze checks for 
//    other media streams. The agent examines the check list for each
//    other media stream in turn:
// 3. If the check list is active, the agent changes the state of
//    all Frozen pairs in that check list whose foundation matches a
//    pair in the valid list under consideration to Waiting.
// 4. If the check list is frozen, and there is at least one pair in
//    the check list whose foundation matches a pair in the valid
//    list under consideration, the state of all pairs in the check
//    list whose foundation matches a pair in the valid list under
//    consideration is set to Waiting. This will cause the check
//    list to become active, and ordinary checks will begin for it.
// 5. If the check list is frozen, and there are no pairs in the
//    check list whose foundation matches a pair in the valid list
//    under consideration, the agent: groups together all of the pairs 
//    with the same foundation, and for each group, sets the state 
//    of the pair with the lowest component ID to Waiting. If there is 
//    more than one such pair, the one with the highest priority is used.

// rfc5245 7.1.3.3. Check List and Timer State Updates (p46)
// If all of the pairs in the check list are now either in the Failed or Succeeded state:
// 1. If there is not a pair in the valid list for each component of the
//    media stream, the state of the check list is set to Failed.
// 2. For each frozen check list, the agent, groups together all of the pairs with the 
//    same foundation, and for each group, sets the state of the pair with the lowest
//    component ID to Waiting. If there is more than one such pair,
//    the one with the highest priority is used.
int ice_checklist_start(struct ice_checklist_t* l, const ice_foundation_t foundation)
{
	int i, j, waiting;
	struct darray_t* component;
	struct ice_candidate_pair_t* pair;

	if (ICE_CHECKLIST_FROZEN == l->state)
		l->state = ICE_CHECKLIST_RUNNING;

	if (ICE_CHECKLIST_RUNNING != l->state)
		return 0;

	waiting = 0;

	// 1. unfreeze pair with same foundation
	for (i = 0; i < darray_count(&l->components); i++)
	{
		component = (struct darray_t*)darray_get(&l->components, i);
		
		for (j = 0; j < darray_count(component); j++)
		{
			pair = (struct ice_candidate_pair_t*)darray_get(component, j);
			if (ICE_CANDIDATE_PAIR_FROZEN == pair->state 
				&& 0 == memcmp(pair->local.foundation, foundation, sizeof(ice_foundation_t)))
				pair->state = ICE_CANDIDATE_PAIR_WAITING;

			if (ICE_CANDIDATE_PAIR_WAITING == pair->state)
				waiting++;
		}
	}

	// 2. at least one waiting state
	if (0 == waiting)
	{
	}
}

int ice_checklist_onbind(struct ice_checklist_t* l, const struct sockaddr_storage* addr)
{
	int r;
	r = darray_find(&l->locals, addr, NULL, socket_addr_compare);
	if (-1 == r)
		return -1;

	return darray_insert2(&l->trigger, addr, socket_addr_compare);
}
