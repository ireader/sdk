#include "ice-agent.h"
#include "ice-internal.h"
#include "stun-agent.h"
#include "stun-internal.h"
#include "ice-checklist.h"
#include "ice-candidates.h"

struct ice_agent_t* ice_create(int controlling, struct ice_agent_handler_t* handler, void* param)
{
	struct ice_agent_t* ice;
	ice = (struct ice_agent_t*)calloc(1, sizeof(struct ice_agent_t));
	if (ice)
	{
		locker_create(&ice->locker);
		LIST_INIT_HEAD(&ice->streams);
		ice->ref = 1;
		ice->param = param;
		ice->nomination = ICE_REGULAR_NOMINATION;
		ice->controlling = controlling;
		ice->tiebreaking = (intptr_t)ice * rand();
		memcpy(&ice->handler, handler, sizeof(ice->handler));
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
	struct ice_stream_t* s;
	struct list_head *ptr, *next;

	ref = atomic_decrement32(&ice->ref);
	assert(ref >= 0);
	if (0 != ref)
		return ref;

	list_for_each_safe(ptr, next, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		ice_stream_destroy(&s);
	}

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

int ice_start(struct ice_agent_t* ice)
{
	struct list_head* ptr, *next;
	struct ice_stream_t* s, *active;

	active = NULL; // default stream
	list_for_each(ptr, next, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		ice_checklist_reset(s->checklist, &s->locals, &s->remotes);
		if (NULL == active && ice_candidates_count(&s->locals) > 0)
			active = s;
	}

	if (NULL == active)
		return -1;
	return ice_checklist_start(active);
}

int ice_stop(struct ice_agent_t* ice)
{
	// TODO
	assert(0);
	return -1;
}

// 7.2. STUN Server Procedures
static int ice_agent_server_onbind(void* param, stun_response_t* resp, const stun_request_t* req)
{
	int r;
	struct ice_agent_t* ice;
	struct ice_stream_t* s;
	struct ice_candidate_t* c;
	struct stun_address_t addr;
	const struct stun_attr_t *nominated, *priority;
	const struct stun_attr_t *controlled, *controlling;

	memset(&addr, 0, sizeof(addr));
	ice = (struct ice_agent_t*)param;
	r = stun_request_getaddr(req, &addr.protocol, &addr.host, &addr.peer, &addr.reflexive, &addr.relay);
	if (0 != r)
		return r;

	c = ice_agent_find_local_candidate(ice, &addr);
	if (!c)
		return -1; // not found

	s = ice_agent_find_stream(ice, c->stream);
	assert(s);

	priority = stun_message_attr_find(&req->msg, STUN_ATTR_PRIORITY);
	nominated = stun_message_attr_find(&req->msg, STUN_ATTR_USE_CANDIDATE);
	controlled = stun_message_attr_find(&req->msg, STUN_ATTR_ICE_CONTROLLED);
	controlling = stun_message_attr_find(&req->msg, STUN_ATTR_ICE_CONTROLLING);
	if (controlled && controlling)
		return 0; // invalid ignore

	// add remote candidates. 
	// However, the agent does not pair this candidate with any local candidates.
	ice_agent_add_remote_peer_reflexive_candidate(ice, &addr, priority);

	// rfc5245 7.2.1.1. Detecting and Repairing Role Conflicts (p47)
	// If the agent is in the controlling role:
	// 1. If the agent's tie-breaker is larger than or equal to the contents of the 
	//    ICE-CONTROLLING attribute, the agent generates a Binding error response and 
	//    includes an ERROR-CODE attribute with a value of 487 (Role Conflict) but retains its role.
	// 2. If the agent's tie-breaker is less than the contents of the ICE-CONTROLLING 
	//    attribute, the agent switches to the controlled role.
	//
	// If the agent is in the controlled role
	// 1. If the agent's tie-breaker is larger than or equal to the contents of 
	//    the ICE-CONTROLLED attribute, the agent switches to the controlling role.
	// 2. If the agent's tie-breaker is less than the contents of the ICE-CONTROLLED 
	//    attribute, the agent generates a Binding error response and includes an 
	//    ERROR-CODE attribute with a value of 487 (Role Conflict) but retains its role.
	if ((controlling && ice->controlling && ice->tiebreaking >= controlled->v.u64)
		|| (controlled && 0 == ice->controlling && ice->tiebreaking < controlled->v.u64))
		return stun_agent_bind_response(resp, ICE_ROLE_CONFLICT, "Role Conflict");

	if ((controlling && ice->controlling && ice->tiebreaking < controlled->v.u64)
		|| (controlled && 0 == ice->controlling && ice->tiebreaking > controlled->v.u64))
	{
		// switch role
		ice->controlling = ice->controlling ? 0 : 1;

		ice_agent_onrole(ice, ice->controlling);
	}

	// try trigger check
	ice_checklist_trigger(s->checklist, &addr, nominated ? 1 : 0);

	return stun_agent_bind_response(resp, 200, "OK");
}

static int ice_agent_server_send(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	struct ice_agent_t* ice;
	ice = (struct ice_agent_t*)param;
	return ice->handler.send(ice->param, protocol, local, remote, data, bytes);
}

static int ice_agent_server_auth(void* param, int cred, const char* usr, const char* realm, const char* nonce, char pwd[512])
{
	// rfc5245 7.1.2. Sending the Request
	// A connectivity check MUST utilize the STUN short-term credential mechanism

	struct ice_agent_t* ice;
	(void)realm, (void)nonce;
	ice = (struct ice_agent_t*)param;
	assert(STUN_CREDENTIAL_SHORT_TERM == cred);
	if(0 != strcmp(usr, ice->auth.usr))
		return -1;
	strcmp(pwd, ice->auth.pwd);
	return 0;
}

static int ice_agent_server_getnonce(void* param, char realm[128], char nonce[128])
{
	assert(0);
	realm[0] = nonce[0] = 0;
	return -1; (void)param;
}

static int ice_agent_init(struct ice_agent_t* ice)
{
	struct stun_agent_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = ice_agent_server_send;
	handler.auth = ice_agent_server_auth;
	handler.onbind = ice_agent_server_onbind;
	handler.getnonce = ice_agent_server_getnonce;
	ice->stun = stun_agent_create(STUN_RFC_5389, &handler, ice);
	return 0;
}
