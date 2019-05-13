#include "ice-checklist.h"
#include "ice-candidates.h"
#include "stun-internal.h"

struct ice_checklist_t* ice_checklist_create(stun_agent_t* stun, const struct stun_credential_t* auth, struct ice_checklist_handler_t* handler, void* param)
{
	struct ice_checklist_t* l;
	l = (struct ice_checklist_t*)calloc(1, sizeof(struct ice_checklist_t));
	if (l)
	{
		memcpy(&l->handler, handler, sizeof(l->handler));
		l->param = param;
		l->timer = NULL;
		l->stun = stun;
		l->auth = auth;
		l->state = ICE_CHECKLIST_FROZEN;
		l->nomination = ICE_REGULAR_NOMINATION;
		darray_init(&l->gathers, sizeof(struct ice_candidate_t), 4);
		ice_candidates_init(&l->locals);
		ice_candidates_init(&l->remotes);
		ice_candidate_pairs_init(&l->trigger);
		ice_candidate_components_init(&l->components);
	}
	return l;
}

int ice_checklist_destroy(struct ice_checklist_t** pl)
{
	struct ice_checklist_t* l;
	if (!pl || !*pl)
		return -1;
	
	l = *pl;
	stun_timer_stop(l->timer);
	ice_candidates_free(&l->locals);
	ice_candidates_free(&l->remotes);
	ice_candidate_pairs_free(&l->trigger);
	ice_candidate_components_free(&l->components);

	free(l);
	*pl = NULL;
	return 0;
}

int ice_checklist_add_local_candidate(struct ice_checklist_t* l, const struct ice_candidate_t* c)
{
	if (AF_INET != c->addr.ss_family && AF_INET6 != c->addr.ss_family)
		return -1;
	if (0 != c->raddr.ss_family && c->addr.ss_family != c->raddr.ss_family)
		return -1;
	if (0 == c->priority || 0 == c->component || 0 == c->foundation[0])
		return -1;
	if (ice_candidates_count(&l->locals) > ICE_CANDIDATE_LIMIT)
		return -1;
	return ice_candidates_insert(&l->locals, c);
}

int ice_checklist_add_remote_candidate(struct ice_checklist_t* l, const struct ice_candidate_t* c)
{
	if (AF_INET != c->addr.ss_family && AF_INET6 != c->addr.ss_family)
		return -1;
	//if (0 != c->stun.ss_family && c->addr.ss_family != c->stun.ss_family)
	//	return -1;
	if (0 == c->priority || 0 == c->component)
		return -1;
	if (ice_candidates_count(&l->remotes) > ICE_CANDIDATE_LIMIT)
		return -1;
	return ice_candidates_insert(&l->remotes, c);
}

int ice_checklist_list_local_candidate(struct ice_checklist_t* l, ice_agent_oncandidate oncand, void* param)
{
	return ice_candidates_list(&l->locals, oncand, param);
}

int ice_checklist_list_remote_candidate(struct ice_checklist_t* l, ice_agent_oncandidate oncand, void* param)
{
	return ice_candidates_list(&l->remotes, oncand, param);
}

int ice_checklist_get_default_candidate(struct ice_checklist_t* l, uint16_t component, struct ice_candidate_t* c)
{
	int i;
	struct ice_candidate_t *p, *pc = NULL;
	for (i = 0; i < ice_candidates_count(&l->locals); i++)
	{
		p = ice_candidates_get(&l->locals, i);
		if(p->component != component)
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

int ice_checklist_build(struct ice_checklist_t* l)
{
	int i, j;
	ice_candidate_pairs_t *component;
	struct ice_candidate_t *local, *remote;
	struct ice_candidate_pair_t pair;

	// reset components
	ice_candidate_components_reset(&l->components);
	
	for (i = 0; i < ice_candidates_count(&l->locals); i++)
	{
		local = ice_candidates_get(&l->locals, i);

		// rfc5245 5.7.3. Pruning the Pairs (p34)
		// For each pair where the local candidate is server reflexive, 
		// the server reflexive candidate MUST be replaced by its base.
		if(ICE_CANDIDATE_HOST != local->type && ICE_CANDIDATE_RELAYED != local->type)
			continue;

		component = ice_candidate_components_fetch(&l->components, local->component);
		if (!component)
			return -1;

		for (j = 0; j < ice_candidates_count(&l->remotes); j++)
		{
			remote = ice_candidates_get(&l->remotes, j);
			if(local->component != remote->component)
				continue;

			// agent MUST limit the total number of connectivity checks the agent  
			// performs across all check lists to a specific value.
			assert(ice_candidate_pairs_count(component) < 64);

			memset(&pair, 0, sizeof(pair));
			pair.state = ICE_CANDIDATE_PAIR_FROZEN;
			ice_candidate_pair_priority(&pair);
			snprintf(pair.foundation, sizeof(pair.foundation), "%s:%s", local->foundation, remote->foundation);
			memcpy(&pair.local, local, sizeof(struct ice_candidate_t));
			memcpy(&pair.remote, remote, sizeof(struct ice_candidate_t));
			ice_candidate_pairs_insert(component, &pair);
		}
	}

	l->state = ICE_CHECKLIST_FROZEN; // reset status
	return 0;
}

static int ice_checklist_onroleconflict(void* param, const stun_request_t* resp, int code, const char* phrase)
{
	// TODO
	assert(0);
	return 0;
}

static void ice_checlist_update_state(struct ice_checklist_t* l)
{
	int i, j, failed;
	ice_candidate_pairs_t* component;
	struct ice_candidate_pair_t *pair;

	failed = 0;
	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		for (j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			if (ICE_CANDIDATE_PAIR_SUCCEEDED != pair->state && ICE_CANDIDATE_PAIR_FAILED != pair->state)
				return; // not finish yet

			if (ICE_CANDIDATE_PAIR_FAILED == pair->state)
				failed++;
		}
	}

	// all pair done
	l->state = 0 == failed ? ICE_CHECKLIST_COMPLETED : ICE_CHECKLIST_FAILED;
	
	// callback/notify
	l->handler.onfinish(l->param, l, l->state);
}

// rfc5245 7.1.3.2.1. Discovering Peer Reflexive Candidates (p43)
static void ice_checlist_add_peer_reflexive(struct ice_checklist_t* l, const stun_request_t* resp)
{
	struct ice_candidate_t c, *local;
	const struct stun_attr_t* priority;
	struct sockaddr_storage remote;

	memset(&c, 0, sizeof(struct ice_candidate_t));
	stun_request_getaddr(resp, &c.protocol, &c.raddr, &remote, &c.addr, NULL);
	priority = stun_message_attr_find(&resp->msg, STUN_ATTR_PRIORITY);

	local = darray_find(&l->locals, &c.raddr, NULL, ice_candidate_compare_host_addr);
	if (NULL == local)
		return; // local not found, new request ???

	c.type = ICE_CANDIDATE_PEER_REFLEXIVE; 
	c.component = local->component;
	ice_candidate_foundation(&c, (struct sockaddr*)&c.addr);
	ice_candidate_priority(&c);
	if (priority)
		c.priority = priority->v.u32;

	ice_checklist_add_local_candidate(l, &c);
}

static int ice_checklist_onbind(void* param, const stun_request_t* req, int code, const char* phrase)
{
	int i, j, r, protocol, failed;
	struct darray_t* component;
	struct ice_checklist_t* l;
	struct ice_candidate_pair_t *pair;
	struct sockaddr_storage local, remote, reflexive;

	l = (struct ice_checklist_t*)param;

	// ice agent callback
	r = stun_request_getaddr(req, &protocol, &local, &remote, &reflexive, NULL);
	if (0 != r)
		return r;

	// Discovering Peer Reflexive Candidates
	ice_checlist_add_peer_reflexive(l, req);

	// completed
	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);

		for (j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			assert(ICE_CANDIDATE_PAIR_INPROGRESS == pair->state);
			if (0 == socket_addr_compare((const struct sockaddr*)&pair->local.raddr, (const struct sockaddr*)&local) 
				&& 0 == socket_addr_compare((const struct sockaddr*)&pair->remote.addr, (const struct sockaddr*)&remote))
			{
				if (0 == code)
				{
					pair->state = ICE_CANDIDATE_PAIR_SUCCEEDED;
					l->handler.onvalid(l->param, l, pair);
				}
				else
				{
					pair->state = ICE_CANDIDATE_PAIR_FAILED;
				}
				break;
			}
		}
	}

	ice_checlist_update_state(l);

	// stream done callback
	return 0;
}

static int ice_checklist_bind(struct ice_checklist_t* l, struct ice_candidate_pair_t *pair)
{
	struct stun_request_t* req;
	req = stun_request_create(l->stun, STUN_RFC_5389, ice_checklist_onbind, l);
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, (const struct sockaddr*)&pair->local.raddr, (const struct sockaddr*)&pair->remote.addr);
	stun_request_setauth(req, req->auth.credential, req->auth.usr, req->auth.pwd, req->auth.realm, req->auth.nonce);
	stun_message_add_uint32(&req->msg, STUN_ATTR_PRIORITY, pair->local.priority);
	if(l->nomination != ICE_REGULAR_NOMINATION) stun_message_add_flag(&req->msg, STUN_ATTR_USE_CANDIDATE);
	stun_message_add_uint64(&req->msg, l->controlling ? STUN_ATTR_ICE_CONTROLLING : STUN_ATTR_ICE_CONTROLLED, rand() * rand());
	return stun_agent_bind(req);
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
static int ice_checklist_ontimer(void* param)
{
	int i, j;
	struct darray_t* component;
	struct ice_candidate_pair_t *pair, *waiting;
	struct ice_checklist_t* l;
	
	l = (struct ice_checklist_t*)param;
	assert(ICE_CHECKLIST_RUNNING == l->state);
	if(ice_candidate_pairs_count(&l->trigger) > 0)
	{
		waiting = ice_candidate_pairs_get(&l->trigger, 0);
		darray_pop_front(&l->trigger);
	}
	else
	{
		waiting = NULL;

		// Find the highest-priority pair in that check list that is in the Waiting state.
		for (i = 0; i < ice_candidate_components_count(&l->components); i++)
		{
			component = ice_candidate_components_get(&l->components, i);
			for (j = 0; j < ice_candidate_pairs_count(component); j++)
			{
				pair = ice_candidate_pairs_get(component, j);
				if (ICE_CANDIDATE_PAIR_WAITING != pair->state)
					continue;
				if (NULL == waiting || pair->local.priority > waiting->local.priority)
					waiting = pair;
			}
		}

		if (!waiting)
		{
			// Find the highest-priority pair in that check list that is in the Frozen state.
			for (i = 0; i < ice_candidate_components_count(&l->components); i++)
			{
				component = ice_candidate_components_get(&l->components, i);
				for (j = 0; j < ice_candidate_pairs_count(component); j++)
				{
					pair = ice_candidate_pairs_get(component, j);
					if(ICE_CANDIDATE_PAIR_FROZEN != pair->state)
						continue;
					if (NULL == waiting || pair->local.priority > waiting->local.priority)
						waiting = pair;
				}
			}
		}
	}

	if (waiting)
	{
		waiting->state = ICE_CANDIDATE_PAIR_INPROGRESS;
		ice_checklist_bind(l, waiting);
	}
	else
	{
		// terminate timer
		stun_timer_stop(l->timer);
		l->timer = NULL;

//		l->state = ICE_CHECKLIST_COMPLETED;
//		l->state = ICE_CHECKLIST_FAILED;
	}

	return 0;
}

// For all pairs with the same foundation, it sets the state of
// the pair with the lowest component ID to Waiting. If there is
// more than one such pair, the one with the highest priority is used.
static void ice_checklist_foundation_group(struct ice_checklist_t* l, struct darray_t* foundations)
{
	int i, j;
	ice_candidate_pairs_t* component;
	struct ice_candidate_pair_t *pair, **pp;

	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		for (j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			if(ICE_CANDIDATE_PAIR_FROZEN != pair->state)
				continue;

			pp = darray_find(foundations, pair, NULL, ice_candidate_pair_compare_foundation);
			if (NULL == pp)
			{
				darray_push_back(foundations, &pair, 1);
				continue;
			}
			
			// update
			if (pair->local.component < (*pp)->local.component)
				*pp = pair;
			else if(pair->local.component == (*pp)->local.component && pair->local.priority >(*pp)->local.priority)
				*pp = pair;
		}
	}
}

// rfc5245 5.7.4. Computing States (p36)
// The initial states for each pair in a check list are computed by
// performing the following sequence of steps:
// 1. The agent sets all of the pairs in each check list to the Frozen state.
// 2. The agent examines the check list for the first media stream, 
//    For all pairs with the same foundation, it sets the state of
//    the pair with the lowest component ID to Waiting. If there is
//    more than one such pair, the one with the highest priority is used.
int ice_checklist_init(struct ice_checklist_t* l)
{
	int i;
	struct darray_t foundations;
	struct ice_candidate_pair_t *pair;

	assert(ICE_CHECKLIST_FROZEN == l->state);
	l->state = ICE_CHECKLIST_RUNNING;
	
	// group with foundation
	darray_init(&foundations, sizeof(struct ice_candidate_pair_t*), 4);
	ice_checklist_foundation_group(l, &foundations);

	for (i = 0; i < darray_count(&foundations); i++)
	{
		pair = *(struct ice_candidate_pair_t**)darray_get(&foundations, i);
		pair->state = ICE_CANDIDATE_PAIR_WAITING;
	}
	darray_free(&foundations);

	// start timer
	assert(NULL == l->timer);
	l->timer = stun_timer_start(ICE_TIMER_INTERVAL, ice_checklist_ontimer, l);
	return 0;
}

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
int ice_checklist_update(struct ice_checklist_t* l, const ice_candidate_pairs_t* valids)
{
	int i, j, waiting;
	struct darray_t* component;
	struct ice_candidate_pair_t* pair;

	// 1. unfreeze pair with same foundation
	for (waiting = i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		for (j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			if (ICE_CANDIDATE_PAIR_FROZEN == pair->state
				&& NULL != darray_find(valids, pair, NULL, ice_candidate_pair_compare_foundation))
			{
				pair->state = ICE_CANDIDATE_PAIR_WAITING;
				waiting++;
			}
		}
	}

	if (ICE_CHECKLIST_RUNNING == l->state)
		return 0;

	// 2. at least one waiting state
	if (0 == waiting)
		return ice_checklist_init(l);

	// start timer
	assert(NULL == l->timer);
	l->timer = stun_timer_start(ICE_TIMER_INTERVAL, ice_checklist_ontimer, l);
	l->state = ICE_CHECKLIST_RUNNING;
	return 0;
}

int ice_checklist_trigger(struct ice_checklist_t* l, int protocol, const struct sockaddr_storage* local, const struct sockaddr_storage* remote)
{
	struct ice_candidate_t *lo, *ro;
	struct ice_candidate_pair_t pair;
	lo = darray_find(&l->locals, local, NULL, ice_candidate_compare_host_addr);
	ro = darray_find(&l->remotes, remote, NULL, ice_candidate_compare_addr);
	if (NULL == lo || NULL == ro)
		return -1;

	memset(&pair, 0, sizeof(pair));
	pair.state = ICE_CANDIDATE_PAIR_WAITING;
	ice_candidate_pair_priority(&pair);
	memcpy(&pair.local, lo, sizeof(struct ice_candidate_t));
	memcpy(&pair.remote, ro, sizeof(struct ice_candidate_t));
	//ice_candidate_pairs_insert(&l->components, &pair);

	return ice_candidate_pairs_insert(&l->trigger, &pair);
}

int ice_checklist_stream_valid(struct ice_checklist_t* l, struct darray_t* valids)
{
	int i, j, waiting;
	struct darray_t* component;
	struct ice_candidate_pair_t* pair;

	// 1. unfreeze pair with same foundation
	for (waiting = i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		for (j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			if (NULL != darray_find(valids, pair, NULL, ice_candidate_pair_compare_foundation))
			{
				waiting++;
				break; // next component
			}
		}
	}

	return waiting == ice_candidate_components_count(&l->components) ? 1 : 0;
}
