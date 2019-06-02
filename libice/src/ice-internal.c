#include "ice-internal.h"
#include "ice-checklist.h"
#include "ice-candidates.h"
#include "stun-internal.h"
#include <assert.h>

static int ice_stun_getnonce(void* param, char realm[128], char nonce[128])
{
	assert(0);
	realm[0] = nonce[0] = 0;
	return -1; (void)param;
}

static int ice_stun_onbind(void* param, stun_response_t* resp, const stun_request_t* req)
{
	int r;
	struct ice_agent_t* ice;
	struct ice_candidate_t* c;
	struct stun_address_t addr;
	struct stun_attr_t *nominated, *priority;
	struct stun_attr_t *controlled, *controlling;
	
	memset(&addr, 0, sizeof(addr));
	ice = (struct ice_agent_t*)param;
	r = stun_request_getaddr(req, &addr.protocol, &addr.host, &addr.peer, &addr.reflexive, &addr.relay);
	if (0 != r)
		return r;

	c = ice_candidates_find(&ice->locals, ice_candidate_compare_host_addr, &addr.host);
	if (!c)
		return -1; // not found

	priority = stun_message_attr_find(&req->msg, STUN_ATTR_PRIORITY);
	nominated = stun_message_attr_find(&req->msg, STUN_ATTR_USE_CANDIDATE);
	controlled = stun_message_attr_find(&req->msg, STUN_ATTR_ICE_CONTROLLED);
	controlling = stun_message_attr_find(&req->msg, STUN_ATTR_ICE_CONTROLLING);
	if (controlled && controlling)
		return 0; // invalid ignore

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

		// TODO: checklist rebuild
	}

	// try trigger check
	if(ice->list[c->stream])
		ice_checklist_trigger(ice->list[c->stream], &addr, nominated ? 1 : 0);

	return stun_agent_bind_response(resp, 200, "OK");
}

static int ice_agent_onrole(void* param, struct ice_checklist_t* l, int controlling)
{
}

static int ice_agent_onvalid(void* param, struct ice_checklist_t* l, const struct ice_candidate_pair_t* pair)
{
	int i;
	struct ice_agent_t* ice;
	struct ice_checklist_t* check;
	ice = (struct ice_agent_t*)param;

	if (0 != ice_candidate_pairs_insert(&ice->valids, pair))
		return -1;

	if (!ice_checklist_stream_valid(l, &ice->valids))
		return 0;

	// sync the other streams
	for (i = 0; i < sizeof(ice->list)/sizeof(ice->list[0]); i++)
	{
		check = ice->list[i];
		if (!check || l == check)
			continue;

		ice_checklist_update(check, &ice->valids);
	}

	return 0;
}

static int ice_agent_onfinish(void* param, struct ice_checklist_t* l, int code)
{
	int i;
	struct ice_agent_t* ice;
	struct ice_checklist_t* check;
	ice = (struct ice_agent_t*)param;

	for (i = 0; i < sizeof(ice->list) / sizeof(ice->list[0]); i++)
	{
		check = ice->list[i];
		if (!check || l == check)
			continue;

		if (!ice_checklist_stream_valid(check, &ice->valids))
			return 0;
	}

	// all stream finish
	// TODO: callback/notify

	return 0;
}

static int ice_agent_refresh_response(void* param, const stun_request_t* req, int code, const char* phrase)
{
	return 0;
}

static void ice_agent_onrefresh(void* param)
{
	int i;
	struct ice_candidate_t *c;
	struct ice_candidate_pair_t *pr;
	struct ice_agent_t* ice;
	ice = (struct ice_agent_t*)param;

	locker_lock(&ice->locker);

	// 1. stun binding/turn refresh
	for (i = 0; i < ice_candidates_count(&ice->locals); i++)
	{
		c = ice_candidates_get(&ice->locals, i);
		if(ICE_CANDIDATE_SERVER_REFLEXIVE == c->type || ICE_CANDIDATE_PEER_REFLEXIVE == c->type)
			ice_agent_bind(ice, &c->host, &c->stun, NULL, ice_agent_refresh_response, ice);
		else if(ICE_CANDIDATE_RELAYED == c->type)
			ice_agent_refresh(ice, &c->host, &c->stun, NULL, ice_agent_refresh_response, ice);
	}

	// 2. valid list
	for (i = 0; i < ice_candidate_pairs_count(&ice->valids); i++)
	{
		pr = ice_candidate_pairs_get(&ice->valids, i);
		if (ICE_CANDIDATE_SERVER_REFLEXIVE == pr->local.type || ICE_CANDIDATE_PEER_REFLEXIVE == pr->local.type)
			ice_agent_bind(ice, &pr->local.host, ice_candidate_addr(&pr->remote), NULL, ice_agent_refresh_response, ice);
		else if (ICE_CANDIDATE_RELAYED == pr->local.type)
			ice_agent_refresh(ice, &pr->local.host, ice_candidate_addr(&pr->remote), &pr->local.relay, ice_agent_refresh_response, ice);
	}

	if (ice)
	{
		ice->timer = stun_timer_start(ICE_BINDING_TIMEOUT, ice_agent_onrefresh, ice);
		if (ice->timer)
			return;
		// TODO: notify start timer error
	}
	locker_unlock(&ice->locker);

	ice_agent_release(ice);
}

static int ice_stun_send(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	struct ice_agent_t* ice;
	ice = (struct ice_agent_t*)param;
	return ice->handler.send(ice->param, protocol, local, remote, data, bytes);
}

static int ice_stun_auth(void* param, int cred, const char* usr, const char* realm, const char* nonce, char pwd[512])
{
	// rfc5245 7.1.2. Sending the Request
	// A connectivity check MUST utilize the STUN short-term credential mechanism

	struct ice_agent_t* ice;
	(void)realm, (void)nonce;
	ice = (struct ice_agent_t*)param;
	assert(STUN_CREDENTIAL_SHORT_TERM == cred);
	return ice->handler.auth(ice->param, usr, pwd);
}

static void ice_agent_ondata(void* param, const void* data, int byte, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay)
{
	struct ice_agent_t* ice = (struct ice_agent_t*)param;
	if (ice && ice->handler.ondata)
		ice->handler.ondata(ice->param, data, byte, protocol, local, remote, relay);
}

int ice_agent_init(struct ice_agent_t* ice)
{
	struct stun_agent_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = ice_stun_send;
	handler.auth = ice_stun_auth;
	handler.onbind = ice_stun_onbind;
	handler.getnonce = ice_stun_getnonce;
	ice->stun = stun_agent_create(STUN_RFC_5389, &handler, ice);
	ice->timer = stun_timer_start(ICE_BINDING_TIMEOUT, ice_agent_onrefresh, ice);
	return 0;
}

struct ice_checklist_t* ice_agent_checklist_create(struct ice_agent_t* ice, int stream)
{
	struct ice_checklist_t* check;
	struct ice_checklist_handler_t h;
	memset(&h, 0, sizeof(h));
	h.onrolechanged = ice_agent_onrole;
	h.onvalidpair = ice_agent_onvalid;
	h.onfinish = ice_agent_onfinish;

	check = ice_checklist_create(ice, &h, ice);
	ice_checklist_build(check, stream, &ice->locals, &ice->remotes);
	return check;
}

int ice_agent_bind(struct ice_agent_t* ice, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, stun_request_handler handler, void* param)
{
	struct stun_request_t* req;
	req = stun_request_create(ice->stun, STUN_RFC_5389, handler, param);
	if (!req) return -1;

	// rfc5245 4.1.1.2. Server Reflexive and Relayed Candidates (p20)
	// Binding requests to a STUN server are not authenticated, and 
	// any ALTERNATESERVER attribute in a response is ignored.
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, local, remote, relay);
	stun_request_setauth(req, ice->auth.credential, ice->auth.usr, ice->auth.pwd, ice->auth.realm, ice->auth.nonce);
	return stun_agent_bind(req);
}

int ice_agent_allocate(struct ice_agent_t* ice, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, stun_request_handler handler, void* param)
{
	struct stun_request_t* req;
	req = stun_request_create(ice->stun, STUN_RFC_5389, handler, param);
	if (!req) return -1;

	// rfc5245 4.1.1.2. Server Reflexive and Relayed Candidates (p20)
	// Allocate requests SHOULD be authenticated using a longterm
	// credential obtained by the client through some other means.
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, local, remote, relay);
	stun_request_setauth(req, ice->auth.credential, ice->auth.usr, ice->auth.pwd, ice->auth.realm, ice->auth.nonce);
	return turn_agent_allocate(req, ice_agent_ondata, ice);
}

int ice_agent_refresh(struct ice_agent_t* ice, const struct sockaddr* local, const struct sockaddr* remote, const struct sockaddr* relay, stun_request_handler handler, void* param)
{
	struct stun_request_t* req;
	req = stun_request_create(ice->stun, STUN_RFC_5389, handler, param);
	if (!req) return -1;

	// rfc5245 4.1.1.2. Server Reflexive and Relayed Candidates (p20)
	// Allocate requests SHOULD be authenticated using a longterm
	// credential obtained by the client through some other means.
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, local, remote, relay);
	stun_request_setauth(req, ice->auth.credential, ice->auth.usr, ice->auth.pwd, ice->auth.realm, ice->auth.nonce);
	return turn_agent_refresh(req, TURN_LIFETIME);
}

int ice_agent_connect(struct ice_agent_t* ice, const struct ice_candidate_pair_t* pr, int nominated, stun_request_handler handler, void* param)
{
	struct stun_request_t* req;
	req = stun_request_create(ice->stun, STUN_RFC_5389, handler, param);
	if (!req) return -1;

	// rfc5245 4.1.1.2. Server Reflexive and Relayed Candidates (p20)
	// Allocate requests SHOULD be authenticated using a longterm
	// credential obtained by the client through some other means.
	stun_request_setaddr(req, STUN_PROTOCOL_UDP, (const struct sockaddr*)&pr->local.host, (const struct sockaddr*)ice_candidate_addr(&pr->remote), ICE_CANDIDATE_RELAYED == pr->local.type ? (const struct sockaddr*)&pr->local.relay : NULL);
	stun_request_setauth(req, ice->auth.credential, ice->auth.usr, ice->auth.pwd, ice->auth.realm, ice->auth.nonce);
	stun_message_add_uint32(&req->msg, STUN_ATTR_PRIORITY, pr->local.priority);
	if (nominated || ice->nomination == ICE_AGGRESSIVE_NOMINATION)
		stun_message_add_flag(&req->msg, STUN_ATTR_USE_CANDIDATE);
	stun_message_add_uint64(&req->msg, ice->controlling ? STUN_ATTR_ICE_CONTROLLING : STUN_ATTR_ICE_CONTROLLED, ice->tiebreaking);
	return stun_agent_bind(req);
}
