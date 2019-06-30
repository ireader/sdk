#include "ice-checklist.h"
#include "ice-candidates.h"
#include "stun-internal.h"

struct ice_checklist_t
{
	int32_t ref;
	struct ice_agent_t* ice;
	enum ice_checklist_state_t state;
	int nominated;

	void* timer;
	ice_candidate_pairs_t trigger; // trigger check list
	ice_candidate_components_t components; // ordinary check, pairs base on component

	struct ice_checklist_handler_t handler;
	void* param;
};

struct ice_checklist_t* ice_checklist_create(struct ice_agent_t* ice, struct ice_checklist_handler_t* handler, void* param)
{
	struct ice_checklist_t* l;
	l = (struct ice_checklist_t*)calloc(1, sizeof(struct ice_checklist_t));
	if (l)
	{
		memcpy(&l->handler, handler, sizeof(l->handler));
		l->param = param;
		l->timer = NULL;
		l->ice = ice;
		l->ref = 1;
		l->state = ICE_CHECKLIST_FROZEN;
		ice_candidate_pairs_init(&l->trigger);
		ice_candidate_components_init(&l->components);
	}
	return l;
}

int ice_checklist_destroy(struct ice_checklist_t** pl)
{
	int32_t ref;
	struct ice_checklist_t* l;
	if (!pl || !*pl)
		return -1;
	
	l = *pl;
	*pl = NULL;
	if (l->timer)
		stun_timer_stop(l->timer);

	ref = atomic_decrement32(&l->ref);
	if (0 != ref)
		return ref;

	ice_candidate_pairs_free(&l->trigger);
	ice_candidate_components_free(&l->components);
	free(l);
	return 0;
}

int ice_checklist_build(struct ice_checklist_t* l, int stream, const ice_candidates_t* locals, const ice_candidates_t* remotes)
{
	int i, j;
	ice_candidate_pairs_t *component;
	struct ice_candidate_t *local, *remote;
	struct ice_candidate_pair_t pair;

	// reset components
	ice_candidate_components_clear(&l->components);
	
	for (i = 0; i < ice_candidates_count(locals); i++)
	{
		local = ice_candidates_get(locals, i);
		if(local->stream != stream)
			continue;

		// rfc5245 5.7.3. Pruning the Pairs (p34)
		// For each pair where the local candidate is server reflexive, 
		// the server reflexive candidate MUST be replaced by its base.
		if(ICE_CANDIDATE_HOST != local->type && ICE_CANDIDATE_RELAYED != local->type)
			continue;

		component = ice_candidate_components_fetch(&l->components, local->component);
		if (!component)
			return -1;

		for (j = 0; j < ice_candidates_count(remotes); j++)
		{
			remote = ice_candidates_get(remotes, j);
			if(local->stream != remote->stream || local->component != remote->component)
				continue;

			// agent MUST limit the total number of connectivity checks the agent  
			// performs across all check lists to a specific value.
			assert(ice_candidate_pairs_count(component) < 64);

			memset(&pair, 0, sizeof(pair));
			pair.state = ICE_CANDIDATE_PAIR_FROZEN;
			pair.nominated = 0; // set on valid check
			pair.controlling = l->ice->controlling;
			memcpy(&pair.local, local, sizeof(struct ice_candidate_t));
			memcpy(&pair.remote, remote, sizeof(struct ice_candidate_t));
			ice_candidate_pair_priority(&pair);
			ice_candidate_pair_foundation(&pair);
			ice_candidate_pairs_insert(component, &pair);
		}
	}

	l->state = ICE_CHECKLIST_FROZEN; // reset status
	return 0;
}

int ice_checklist_onrolechanged(struct ice_checklist_t* l, int controlling)
{
	int i, j;
	ice_candidate_pairs_t* component;
	struct ice_candidate_pair_t* pair;

	// update pair priority
	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		for (j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			pair->controlling = controlling;
			ice_candidate_pair_priority(pair);
		}
	}

	return 0;
}

/// @return ICE_CANDIDATE_PAIR_FAILED/ICE_CANDIDATE_PAIR_SUCCEEDED all done, other frozen/waiting/progress
static int ice_checklist_stream_status(struct ice_checklist_t* l)
{
	int i, j, failed;
	ice_candidate_pairs_t* component;
	struct ice_candidate_pair_t* pair;

	// 1. unfreeze pair with same foundation
	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		for (failed = j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			if (ICE_CANDIDATE_PAIR_SUCCEEDED == pair->state)
				break;
			else if (ICE_CANDIDATE_PAIR_FAILED == pair->state)
				++failed;
			else
				continue; // running
		}

		if (failed == ice_candidate_pairs_count(component))
		{
			return ICE_CANDIDATE_PAIR_FAILED;
		}
		else if(j >= ice_candidate_pairs_count(component))
		{
			return ICE_CANDIDATE_PAIR_INPROGRESS;
		}
	}

	return ICE_CANDIDATE_PAIR_SUCCEEDED;
}

/// @return 
static int ice_checklist_status(struct ice_checklist_t* l)
{
	int i, j, failed;
	ice_candidate_pairs_t* component;
	struct ice_candidate_pair_t* pair;

	if (ICE_CHECKLIST_FROZEN == l->state)
		return ICE_CHECKLIST_FROZEN;

	// 1. unfreeze pair with same foundation
	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		for (failed = j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			if (pair->nominated)
			{
				assert(ICE_CANDIDATE_PAIR_SUCCEEDED == pair->state);
				break; // next component
			}
			else if (ICE_CANDIDATE_PAIR_FAILED == pair->state)
			{
				++failed;
			}
		}

		if (failed == ice_candidate_pairs_count(component))
		{
			return ICE_CHECKLIST_FAILED;
		}
		else if (j >= ice_candidate_pairs_count(component))
		{
			return ICE_CHECKLIST_RUNNING;
		}
	}

	return ICE_CHECKLIST_COMPLETED;
}

int ice_checklist_running(struct ice_checklist_t* l)
{
	return ICE_CHECKLIST_RUNNING == l->state ? 1 : 0;
}

static void ice_checlist_update_state(struct ice_checklist_t* l)
{
	l->state = ice_checklist_status(l);
	switch (l->state)
	{
	case ICE_CHECKLIST_COMPLETED:
	case ICE_CHECKLIST_FAILED:
		// callback/notify ok
		l->handler.onfinish(l->param, l, l->state);
		break;

	default:
		switch (ice_checklist_stream_status(l))
		{
		case ICE_CANDIDATE_PAIR_FAILED:
			// callback/notify failed
			l->handler.onfinish(l->param, l, l->state);
			break;

		case ICE_CANDIDATE_PAIR_SUCCEEDED:
			// do nothing, need nominated
			break;

		default:
			break;
		}
	}
}

// rfc5245 7.1.3.2.1. Discovering Peer Reflexive Candidates (p43)
static void ice_checlist_add_peer_reflexive(struct ice_checklist_t* l, const stun_request_t* resp)
{
	struct ice_candidate_t c, *local;
	const struct stun_attr_t* priority;

	memset(&c, 0, sizeof(struct ice_candidate_t));
	stun_request_getaddr(resp, &c.protocol, &c.host, &c.stun, &c.reflexive, &c.relay);
	priority = stun_message_attr_find(&resp->msg, STUN_ATTR_PRIORITY);
	
	local = ice_candidates_find(&l->ice->locals, ice_candidate_compare_host_addr, &c.host);
	if (NULL == local)
		return; // local not found, new request ???

	assert(0 == c.relay.ss_family);
	c.type = ICE_CANDIDATE_PEER_REFLEXIVE;
	c.stream = local->stream;
	c.component = local->component;
	ice_candidate_foundation(&c);
	ice_candidate_priority(&c);
	if (priority)
		c.priority = priority->v.u32;
	ice_add_local_candidate(l->ice, &c);
}

static int ice_checklist_onbind(void* param, const stun_request_t* req, int code, const char* phrase)
{
	int r;
	struct ice_checklist_t* l;
	struct ice_candidate_pair_t *pair;
	struct stun_address_t addr;
	const struct stun_attr_t *nominated, *priority;
	const struct stun_attr_t *controlled, *controlling;

	l = (struct ice_checklist_t*)param;

	// ice agent callback
	memset(&addr, 0, sizeof(addr));
	r = stun_request_getaddr(req, &addr.protocol, &addr.host, &addr.peer, &addr.reflexive, &addr.relay);
	if (0 != r)
		return r;

	priority = stun_message_attr_find(&req->msg, STUN_ATTR_PRIORITY);
	nominated = stun_message_attr_find(&req->msg, STUN_ATTR_USE_CANDIDATE);
	controlled = stun_message_attr_find(&req->msg, STUN_ATTR_ICE_CONTROLLED);
	controlling = stun_message_attr_find(&req->msg, STUN_ATTR_ICE_CONTROLLING);

	// Discovering Peer Reflexive Candidates
	ice_checlist_add_peer_reflexive(l, req);

	// completed
	pair = ice_candidate_components_find(&l->components, &addr);
	if (!pair)
	{
		assert(0); // test only, should be remove in feature
		return 0; // ignore
	}

	assert(ICE_CANDIDATE_PAIR_INPROGRESS == pair->state);
	if (0 == code)
	{
		pair->nominated = nominated ? 1 : 0;
		pair->state = ICE_CANDIDATE_PAIR_SUCCEEDED;
		l->handler.onvalidpair(l->param, l, pair, ice_checklist_stream_status(l));
	}
	else if (ICE_ROLE_CONFLICT == code)
	{
		// rfc5245 7.1.3.1. Failure Cases (p42)
		if ((l->ice->controlling && controlling) || (0 == l->ice->controlling && controlled))
		{
			l->ice->controlling = l->ice->controlling ? 0 : 1;
			l->handler.onrolechanged(l->param, l, l->ice->controlling);
		}
		// else ignore
	}
	else
	{
		assert(0); // connective ok, what's wrong???
		pair->state = ICE_CANDIDATE_PAIR_FAILED;
	}

	ice_checlist_update_state(l);

	// stream done callback
	return 0;
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
	int i, j, nominated;
	ice_candidate_pairs_t* component;
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
		assert(ICE_CANDIDATE_PAIR_FAILED != waiting->state);
		nominated = ICE_CANDIDATE_PAIR_SUCCEEDED == waiting->state ? l->nominated : 0;
		waiting->state = ICE_CANDIDATE_PAIR_SUCCEEDED == waiting->state ? ICE_CANDIDATE_PAIR_SUCCEEDED : ICE_CANDIDATE_PAIR_INPROGRESS;
		ice_agent_connect(l->ice, waiting, nominated, ice_checklist_onbind, l);
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
int ice_checklist_start(struct ice_checklist_t* l)
{
	int i;
	struct darray_t foundations;
	struct ice_candidate_pair_t *pair;

	assert(ICE_CHECKLIST_FROZEN == l->state);
	l->state = ICE_CHECKLIST_RUNNING;
	
	// group with foundation
	memset(&foundations, 0, sizeof(foundations));
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
		return ice_checklist_start(l);

	// start timer
	assert(NULL == l->timer);
	l->timer = stun_timer_start(ICE_TIMER_INTERVAL, ice_checklist_ontimer, l);
	l->state = ICE_CHECKLIST_RUNNING;
	return 0;
}

int ice_checklist_trigger(struct ice_checklist_t* l, const struct stun_address_t* addr, int nominated)
{
	struct ice_candidate_pair_t *pair, *reflexive;
	
	// 1. unfreeze pair with same foundation
	pair = ice_candidate_components_find(&l->components, addr);
	if (!pair)
	{
		// bind request from unknown internal NAT, response ok
		// REMOTER: add peer reflexive address
		// TODO: re-invite to update remote address(new peer reflexive address)???
		assert(0);
		return 0;
	}

	pair->nominated = pair->nominated ? 1 : nominated;
	ice_candidate_pairs_insert(&l->trigger, pair);
	ice_checlist_update_state(l);
	return 0;
}

int ice_checklist_conclude(struct ice_checklist_t* l)
{
	int i, j;
	struct darray_t* component;
	struct ice_candidate_pair_t* pair;

	l->nominated = 1;

	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		for (j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			if (ICE_CANDIDATE_PAIR_SUCCEEDED == pair->state)
			{
				// TODO: choose highest priority pair
				ice_candidate_pairs_insert(&l->trigger, pair);
				break; // next component
			}
		}
	}

	return 0;
}
