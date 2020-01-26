#include "ice-checklist.h"
#include "ice-candidates.h"
#include "stun-internal.h"

struct ice_checklist_t
{
	int32_t ref;
	locker_t locker;
	struct ice_agent_t* ice;
	enum ice_checklist_state_t state;
	int conclude;

	void* timer;
	struct darray_t valids; // valid list
	struct darray_t trigger; // trigger check list
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
		l->ref = 1;
		l->ice = ice;
		l->param = param;
		locker_create(&l->locker);
		memcpy(&l->handler, handler, sizeof(l->handler));
		
		l->state = ICE_CHECKLIST_FROZEN;
		darray_init(&l->valids, sizeof(struct ice_candidate_pair_t*), 16);
		darray_init(&l->trigger, sizeof(struct ice_candidate_pair_t*), 16);
		ice_candidate_components_init(&l->components);
	}
	return l;
}

static int ice_checklist_addref(struct ice_checklist_t* l)
{
	assert(l->ref > 0);
	return atomic_increment32(&l->ref);
}

static int ice_checklist_release(struct ice_checklist_t* l)
{
	int32_t ref;
	ref = atomic_decrement32(&l->ref);
	assert(ref >= 0);
	if (0 != ref)
		return ref;

	darray_free(&l->valids);
	darray_free(&l->trigger);
	ice_candidate_components_free(&l->components);
	locker_destroy(&l->locker);
	free(l);
	return 0;
}

int ice_checklist_destroy(struct ice_checklist_t** pl)
{
	int32_t ref;
	struct ice_checklist_t* l;
	if (!pl || !*pl)
		return -1;
	
	l = *pl;
	*pl = NULL;

	if (l->timer && 0 == stun_timer_stop(l->timer))
	{
		ref = atomic_decrement32(&l->ref);
		assert(ref > 0);
	}

	return ice_checklist_release(l);
}

static int ice_checklist_onpermission(void* param, const stun_request_t* req, int code, const char* phrase)
{
	int r;
	struct ice_checklist_t* l;
	struct stun_address_t addr;

	if (0 == code)
	{
		memset(&addr, 0, sizeof(addr));
		l = (struct ice_checklist_t*)param; (void)phrase;
		r = stun_request_getaddr(req, &addr.protocol, &addr.host, &addr.peer, &addr.reflexive, &addr.relay);
		if (0 != r)
		{
			assert(0);
			ice_checklist_release(l);
			return r;
		}
	}
	return 0;
}


static int ice_checklist_add_candidate_pair(struct ice_checklist_t* l, struct ice_stream_t* stream, const struct ice_candidate_t* local, const struct ice_candidate_t* remote, ice_candidate_pairs_t *component)
{
	int r;
	struct ice_candidate_pair_t pair;

	assert(local->stream == remote->stream);
	if (local->protocol != remote->protocol || local->stream != remote->stream || local->component != remote->component || local->addr.ss_family != remote->addr.ss_family)
		return 0; // skip

	if (ICE_CANDIDATE_RELAYED == local->type)
	{
		ice_checklist_addref(l);
		r = ice_agent_create_permission(l->ice, &l->ice->sauth, (const struct sockaddr*)&local->host, (const struct sockaddr*)&l->ice->saddr, (const struct sockaddr*)&remote->addr, 2000, ice_checklist_onpermission, l);
		if (0 != r)
		{
			assert(0);
			ice_checklist_release(l);

			// ignore
		}
	}

	// agent MUST limit the total number of connectivity checks the agent  
	// performs across all check lists to a specific value.
	assert(ice_candidate_pairs_count(component) < 64);

	memset(&pair, 0, sizeof(pair));
	pair.stream = stream;
	pair.state = ICE_CANDIDATE_PAIR_FROZEN;
	memcpy(&pair.local, local, sizeof(struct ice_candidate_t));
	memcpy(&pair.remote, remote, sizeof(struct ice_candidate_t));
	ice_candidate_pair_priority(&pair, l->ice->controlling);
	ice_candidate_pair_foundation(&pair);
	return ice_candidate_pairs_insert(component, &pair);
}

int ice_checklist_reset(struct ice_checklist_t* l, struct ice_stream_t* stream, const ice_candidates_t* locals, const ice_candidates_t* remotes)
{
	int r, i, j;
	ice_candidate_pairs_t *component;
	struct ice_candidate_t *local, *remote;

	locker_lock(&l->locker);
	if (ICE_CHECKLIST_FROZEN != l->state)
		stun_timer_stop(l->timer);
	l->conclude = 0;
	l->state = ICE_CHECKLIST_FROZEN;
	darray_clear(&l->valids);
	darray_clear(&l->trigger);
	ice_candidate_components_clear(&l->components); // reset components

	for (r = i = 0; i < ice_candidates_count(locals); i++)
	{
		local = ice_candidates_get((ice_candidates_t*)locals, i);

		// rfc5245 5.7.3. Pruning the Pairs (p34)
		// For each pair where the local candidate is server reflexive, 
		// the server reflexive candidate MUST be replaced by its base.
		if(ICE_CANDIDATE_HOST != local->type && ICE_CANDIDATE_RELAYED != local->type)
			continue; // TODO: compare SERVER_REFLEXIVE priority

		component = ice_candidate_components_fetch(&l->components, local->component);
		if (!component)
		{
			r = -1;
			break;
		}

		for (j = 0; j < ice_candidates_count(remotes); j++)
		{
			remote = ice_candidates_get((ice_candidates_t*)remotes, j);
			r = ice_checklist_add_candidate_pair(l, stream, local, remote, component);
			assert(0 == r);
		}
	}

	locker_unlock(&l->locker);
	return r;
}

int ice_checklist_onrolechanged(struct ice_checklist_t* l, int controlling)
{
	int i, j;
	ice_candidate_pairs_t* component;
	struct ice_candidate_pair_t* pair;

	// 1. peer request, 2. local response
	locker_lock(&l->locker);
	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		for (j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			// update pair priority
			ice_candidate_pair_priority(pair, controlling);
		}
	}
	locker_unlock(&l->locker);

	return 0;
}

/// @return ICE_CANDIDATE_PAIR_SUCCEEDED/ICE_CANDIDATE_PAIR_FAILED/ICE_CANDIDATE_PAIR_INPROGRESS
static int ice_checklist_get_component_status(ice_candidate_pairs_t* component)
{
	int i, failed;
	struct ice_candidate_pair_t* pair;

	for (failed = i = 0; i < ice_candidate_pairs_count(component); i++)
	{
		pair = ice_candidate_pairs_get(component, i);
		switch (pair->state)
		{
		case ICE_CANDIDATE_PAIR_SUCCEEDED:
			return ICE_CANDIDATE_PAIR_SUCCEEDED;
		case ICE_CANDIDATE_PAIR_FAILED:
			failed++;
			break;
		default:
			break; // do nothing
		}
	}

	return failed == ice_candidate_pairs_count(component) ? ICE_CANDIDATE_PAIR_FAILED : ICE_CANDIDATE_PAIR_INPROGRESS;
}

/// @return ICE_CANDIDATE_PAIR_SUCCEEDED/ICE_CANDIDATE_PAIR_FAILED/ICE_CANDIDATE_PAIR_INPROGRESS
static int ice_checklist_get_stream_status(struct ice_checklist_t* l)
{
	int i;
	ice_candidate_pairs_t* component;

	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		switch (ice_checklist_get_component_status(component))
		{
		case ICE_CANDIDATE_PAIR_SUCCEEDED:
			break; // continue
		case ICE_CANDIDATE_PAIR_FAILED:
			return ICE_CANDIDATE_PAIR_FAILED;
		default:
			return ICE_CANDIDATE_PAIR_INPROGRESS;
		}
	}

	return ICE_CANDIDATE_PAIR_SUCCEEDED;
}

int ice_checklist_getnominated(struct ice_checklist_t* l, struct ice_candidate_pair_t *components, int n)
{
	int i, j;
	ice_candidate_pairs_t* component;
	struct ice_candidate_pair_t* pair;

	if (n < ice_candidate_components_count(&l->components))
	{
		assert(0);
		return -1;
	}

	assert(l->conclude || 0 == l->ice->controlling);
	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		for (j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			if (pair->nominated)
			{
				memcpy(&components[i], pair, sizeof(struct ice_candidate_pair_t));
				break;
			}
		}
	}

	return ice_candidate_components_count(&l->components);
}

static int ice_checklist_is_nominated(struct ice_checklist_t* l)
{
	int i, j;
	ice_candidate_pairs_t* component;
	struct ice_candidate_pair_t* pair;

	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		for (j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			if (pair->nominated)
				break;
		}

		if (j >= ice_candidate_pairs_count(component))
			return 0;
	}

	return 1;
}

int ice_checklist_getstatus(struct ice_checklist_t* l)
{
	return l->state;
}

static void ice_checklist_update_state(struct ice_checklist_t* l)
{
	int state;
	if(ice_checklist_is_nominated(l))
	{
		l->state = ICE_CHECKLIST_COMPLETED;

		// callback/notify ok
		l->handler.oncomplete(l->param, l, 0);
	}
	else
	{
		state = ice_checklist_get_stream_status(l);
		if (ICE_CANDIDATE_PAIR_SUCCEEDED == state)
		{
			// callback/notify ok
			l->handler.onbind(l->param, l, &l->valids);
		}
		else if (ICE_CANDIDATE_PAIR_FAILED == state)
		{
			l->state = ICE_CHECKLIST_FAILED;
			// callback/notify ok
			l->handler.oncomplete(l->param, l, 1);
		}
		else
		{
			// nothing to do
		}
	}
}

static void ice_checklist_update_foundation(struct ice_checklist_t* l, const struct ice_candidate_pair_t* pr);
static int ice_checklist_onbind(void* param, const stun_request_t* req, int code, const char* phrase)
{
	int r;
	struct ice_checklist_t* l;
	struct ice_candidate_pair_t *pair;
	struct stun_address_t addr;
	const struct stun_attr_t *nominated, *priority;
	const struct stun_attr_t *controlled, *controlling;

	l = (struct ice_checklist_t*)param; (void)phrase;

	// ice agent callback
	memset(&addr, 0, sizeof(addr));
	r = stun_request_getaddr(req, &addr.protocol, &addr.host, &addr.peer, &addr.reflexive, &addr.relay);
	if (0 != r)
	{
		assert(0);
		ice_checklist_release(l);
		return r;
	}

	priority = stun_message_attr_find(&req->msg, STUN_ATTR_PRIORITY);
	nominated = stun_message_attr_find(&req->msg, STUN_ATTR_USE_CANDIDATE);
	controlled = stun_message_attr_find(&req->msg, STUN_ATTR_ICE_CONTROLLED);
	controlling = stun_message_attr_find(&req->msg, STUN_ATTR_ICE_CONTROLLING);

	locker_lock(&l->locker);

	// rfc5245 7.1.3.2.1. Discovering Peer Reflexive Candidates
	ice_agent_add_peer_reflexive_candidate(l->ice, &addr, priority);

	// completed
	pair = ice_candidate_components_find(&l->components, &addr);
	if (!pair)
	{
		assert(0); // test only, should be remove in future
		locker_unlock(&l->locker);
		ice_checklist_release(l);
		return 0; // ignore
	}
#if defined(_DEBUG) || defined(DEBUG)
	{
		char ip[256];
		printf("[C] ice [%d:%d] %s%s [%s:%hu] -> [%s:%hu] ==> %d\n", (int)pair->local.stream, (int)pair->local.component, controlling?"[controlling]":"[controlled]", nominated?"[nominated]":"", IP(&pair->local.host, ip), PORT(&pair->local.host), IP(&pair->remote.addr, ip + 65), PORT(&pair->remote.addr), code);
	}
#endif

	// TODO: multi-onbind response(by trigger)
	//assert(ICE_CANDIDATE_PAIR_INPROGRESS == pair->state || ICE_CANDIDATE_PAIR_SUCCEEDED == pair->state /*norminated*/);
	if (0 == code)
	{
		ice_checklist_update_foundation(l, pair);

		pair->nominated = (pair->nominated || nominated) ? 1 : 0;
		pair->state = ICE_CANDIDATE_PAIR_SUCCEEDED;
		darray_insert2(&l->valids, &pair, NULL);
		ice_checklist_update_state(l);
	}
	else if (ICE_ROLE_CONFLICT == code)
	{
		// rfc5245 7.1.3.1. Failure Cases (p42)
		if ((l->ice->controlling && controlling) || (0 == l->ice->controlling && controlled))
		{
			l->ice->controlling = l->ice->controlling ? 0 : 1;
			l->handler.onrolechanged(l->param);
		}
		// else ignore

		// redo connectivity check
		pair->state = ICE_CANDIDATE_PAIR_WAITING;
		darray_insert2(&l->trigger, &pair, NULL);
	}
	else
	{
		// timeout
		pair->state = ICE_CANDIDATE_PAIR_FAILED;
		ice_checklist_update_state(l);
	}

	// stream done callback
	locker_unlock(&l->locker);
	ice_checklist_release(l);
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
static void ice_checklist_ontimer(void* param)
{
	int i, j, nominated;
	ice_candidate_pairs_t* component;
	struct ice_candidate_pair_t *pair, *waiting;
	struct ice_checklist_t* l;
	
	l = (struct ice_checklist_t*)param;
	locker_lock(&l->locker);
	assert(ICE_CHECKLIST_FROZEN != l->state);

	waiting = NULL;
	if(ice_candidate_pairs_count(&l->trigger) > 0)
	{
		waiting = *(struct ice_candidate_pair_t**)darray_get(&l->trigger, 0);
		darray_pop_front(&l->trigger);
	}
	else if(0 == l->conclude)
	{
		// Find the highest-priority pair in that check list that is in the Waiting state.
		for (i = 0; i < ice_candidate_components_count(&l->components); i++)
		{
			component = ice_candidate_components_get(&l->components, i);
			for (j = 0; j < ice_candidate_pairs_count(component); j++)
			{
				pair = ice_candidate_pairs_get(component, j);
				if (ICE_CANDIDATE_PAIR_WAITING != pair->state)
					continue;
				if (NULL == waiting || pair->priority > waiting->priority)
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
					if (NULL == waiting || pair->priority > waiting->priority)
						waiting = pair;
				}
			}
		}
	}
	else {
		assert(0 != l->conclude);
		// do nothing
	}

	if (waiting)
	{
		ice_checklist_addref(l);
		assert(ICE_CANDIDATE_PAIR_FAILED != waiting->state);
		nominated = ICE_CANDIDATE_PAIR_SUCCEEDED == waiting->state ? l->conclude : 0;
		waiting->state = ICE_CANDIDATE_PAIR_SUCCEEDED == waiting->state ? ICE_CANDIDATE_PAIR_SUCCEEDED : ICE_CANDIDATE_PAIR_INPROGRESS;
		if (0 != ice_agent_connect(l->ice, waiting, nominated, STUN_TIMEOUT, ice_checklist_onbind, l))
		{
			waiting->state = ICE_CANDIDATE_PAIR_FAILED;
			ice_checklist_update_state(l);
			ice_checklist_release(l);
		}
	}
	else
	{
		// nothing to do, waiting for bind response or trigger check
	}

	if (ICE_CHECKLIST_RUNNING == l->state)
	{
		// timer next-tick
		ice_checklist_addref(l);
		l->timer = stun_timer_start(ICE_TIMER_INTERVAL * ice_agent_active_checklist_count(l->ice), ice_checklist_ontimer, l);
	}

	locker_unlock(&l->locker);
	ice_checklist_release(l);
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
			assert(ICE_CANDIDATE_PAIR_FROZEN == pair->state);
			pp = darray_find(foundations, pair, NULL, (darray_compare)ice_candidate_pair_compare_foundation);
			if (NULL == pp)
			{
				darray_insert(foundations, -1, &pair);
				continue;
			}
			
			// update
			if (pair->local.component < (*pp)->local.component)
				*pp = pair;
			else if(pair->local.component == (*pp)->local.component && pair->priority >(*pp)->priority)
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
int ice_checklist_start(struct ice_checklist_t* l, int first)
{
	int i;
	struct darray_t foundations;
	struct ice_candidate_pair_t *pair;

	assert(ICE_CHECKLIST_FROZEN == l->state);
	l->state = ICE_CHECKLIST_RUNNING;
	
	if (first)
	{
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
	}

	// start timer
	assert(NULL == l->timer);
	ice_checklist_addref(l);
	l->timer = stun_timer_start(ICE_TIMER_INTERVAL * ice_agent_active_checklist_count(l->ice), ice_checklist_ontimer, l);
	return 0;
}

int ice_checklist_cancel(struct ice_checklist_t* l)
{
	assert(0);
	return -1;
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
int ice_checklist_update(struct ice_checklist_t* l, const struct darray_t* valids)
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
			if ( ICE_CANDIDATE_PAIR_FROZEN == pair->state
				&& NULL != darray_find(valids, pair, NULL, (darray_compare)ice_candidate_pair_compare_foundation))
			{
				pair->state = ICE_CANDIDATE_PAIR_WAITING;
				waiting++;
			}
		}
	}

	// 1. running checklist
	if (ICE_CHECKLIST_RUNNING == l->state)
		return 0;

	// 2. active frozen checklist(0 == waiting or not)
	if (ICE_CHECKLIST_FROZEN == l->state)
		return ice_checklist_start(l, 0);

	//ICE_CHECKLIST_COMPLETED, ICE_CHECKLIST_FAILED;
	assert(0 == waiting);
	return 0;
}

// rfc5245 7.1.3.2.3. Updating Pair States (45)
// 1. The agent changes the states for all other Frozen pairs for the
//    same media stream and same foundation to Waiting. Typically, but
//    not always, these other pairs will have different component IDs.
static void ice_checklist_update_foundation(struct ice_checklist_t* l, const struct ice_candidate_pair_t* pr)
{
	int i, j;
	struct darray_t* component;
	struct ice_candidate_pair_t* frozen;

	// 1. unfreeze pair with same foundation
	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		component = ice_candidate_components_get(&l->components, i);
		for (j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			frozen = ice_candidate_pairs_get(component, j);
			assert(0 != strcmp(frozen->foundation, pr->foundation) || ICE_CANDIDATE_PAIR_FAILED != frozen->state);
			if (ICE_CANDIDATE_PAIR_FROZEN == frozen->state && 0 == strcmp(frozen->foundation, pr->foundation))
				frozen->state = ICE_CANDIDATE_PAIR_WAITING;
		}
	}
}

int ice_checklist_trigger(struct ice_checklist_t* l, struct ice_stream_t* stream, const struct ice_candidate_t* local, const struct stun_address_t* addr, int nominated)
{
	ice_candidate_pairs_t *component;
	struct ice_candidate_t* remote;
	struct ice_candidate_pair_t *pair;
	
	locker_lock(&l->locker);
	component = ice_candidate_components_fetch(&l->components, local->component);
	if (!component)
	{
		// assert(0); // ignore before local init
		locker_unlock(&l->locker);
		return 0;
	}

	pair = ice_candidate_pairs_find(component, ice_candidate_pair_compare_addr, addr);
	if (!pair)
	{
		// bind request from unknown internal NAT, response ok
		// REMOTER: add peer reflexive address
		// TODO: re-invite to update remote address(new peer reflexive address)???
		remote = ice_agent_find_remote_candidate(l->ice, &addr->peer);
		if (!remote || 0 != ice_checklist_add_candidate_pair(l, stream, local, remote, component))
		{
			locker_unlock(&l->locker);
			return 0;
		}
		pair = ice_candidate_pairs_find(component, ice_candidate_pair_compare_addr, addr);
		assert(pair);
	}

	// 7.2. STUN Server Procedures
	// 7.2.1.4. Triggered Checks (p49)
	// 1. If the state of that pair is Waiting or Frozen, a check for that pair is 
	//    enqueued into the triggered check queue if not already present.
	// 2. If the state of that pair is In-Progress, the agent cancels the in-progress transaction.
	//    In addition, the agent MUST create a new connectivity check for that pair
	// 3. If the state of the pair is Failed, it is changed to Waiting and the agent 
	//    MUST create a new connectivity check for that pair
	// 4. If the state of that pair is Succeeded, nothing further is done.
	if (ICE_CANDIDATE_PAIR_SUCCEEDED != pair->state)
	{
		if (ICE_CHECKLIST_FAILED == l->state)
			l->state = ICE_CHECKLIST_RUNNING;
		//if (ICE_CANDIDATE_PAIR_INPROGRESS == pair->state)
		//	stun_request_cancel(req);
		darray_insert2(&l->trigger, &pair, NULL);
	}
	
	// 7.2.1.5. Updating the Nominated Flag (p50)
	// 1. If the state of this pair is Succeeded, The agent now sets the nominated flag in the valid pair to true.
	// 2. If the state of this pair is In-Progress, if its check produces a successful result, 
	//    the resulting valid pair has its nominated flag set when the response arrives.
	if(0 == l->ice->controlling /*&& l->ice->nomination != ICE_AGGRESSIVE_NOMINATION*/ )
		pair->nominated = pair->nominated ? 1 : nominated;

	// 8.1.2. Updating States (p53)
	// 1. If there are no nominated pairs in the valid list for a media stream and the state 
	//    of the check list is Running, ICE processing continues.
	// 2. If there is at least one nominated pair in the valid list for a media stream and 
	//    the state of the check list is Running :
	//    * The agent MUST remove all Waiting and Frozen pairs in the check list and triggered 
	//      check queue for the same component as the nominated pairs for that media stream.
	//    * If an In-Progress pair in the check list is for the same component as a nominated pair, 
	//      the agent SHOULD cease retransmissions for its check if its pair priority is lower
	//      than the lowest-priority nominated pair for that component.
	// 3. Once there is at least one nominated pair in the valid list for every component of at 
	//    least one media stream and the state of the check list is Running:
	//    * The agent MUST change the state of processing for its check list for that media stream to Completed.
	//    * The agent MUST continue to respond to any checks it may still receive for that media stream, 
	//      and MUST perform triggered checks if required by the processing of Section 7.2.
	//    * The agent MUST continue retransmitting any In-Progress checks for that check list.
	//    * The agent MAY begin transmitting media for this media stream as described in Section 11.1.
	// 4. Once the state of each check list is Completed:
	//    * The agent sets the state of ICE processing overall to Completed.
	//    * If an agent is controlling, it examines the highest-priority nominated candidate pair for each 
	//      component of each media stream. If any of those candidate pairs differ from the default candidate 
	//      pairs in the most recent offer/answer exchange, the controlling agent MUST generate an updated offer
	//      as described in Section 9. If the controlling agent is using an aggressive nomination algorithm, 
	//      this may result in several updated offers as the pairs selected for media change. An agent MAY delay 
	//      sending the offer for a brief interval (one second is RECOMMENDED) in order to allow the selected pairs to stabilize.
	// 5. If the state of the check list is Failed, ICE has not been able to complete for this media stream. 
	//    The correct behavior depends on the state of the check lists for other media streams:
	//    * If all check lists are Failed, ICE processing overall is considered to be in the Failed state, 
	//      and the agent SHOULD consider the session a failure, SHOULD NOT restart ICE, and the controlling 
	//      agent SHOULD terminate the entire session.
	//    * If at least one of the check lists for other media streams is Completed, the controlling agent 
	//      SHOULD remove the failed media stream from the session in its updated offer.
	//    * If none of the check lists for other media streams are Completed, but at least one is Running, 
	//      the agent SHOULD let ICE continue
	ice_checklist_update_state(l);
	locker_unlock(&l->locker);
	return 0;
}

int ice_checklist_conclude(struct ice_checklist_t* l)
{
	int i, j;
	struct darray_t* component;
	struct ice_candidate_pair_t* pair, *pair0;

	if (!l->ice->controlling || l->conclude)
		return 0;
	l->conclude = 1;

	for (i = 0; i < ice_candidate_components_count(&l->components); i++)
	{
		pair0 = NULL;
		component = ice_candidate_components_get(&l->components, i);
		for (j = 0; j < ice_candidate_pairs_count(component); j++)
		{
			pair = ice_candidate_pairs_get(component, j);
			if (ICE_CANDIDATE_PAIR_SUCCEEDED == pair->state && (NULL == pair0 || pair0->priority < pair->priority))
			{
				// choose highest priority pair
				pair0 = pair;
			}
		}

		assert(pair0);
		if (pair0)
			darray_insert2(&l->trigger, &pair0, NULL);
	}

	return 0;
}
