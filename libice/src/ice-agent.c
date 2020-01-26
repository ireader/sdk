#include "ice-internal.h"
#include "stun-internal.h"
#include "ice-checklist.h"
#include "ice-candidates.h"

static int ice_agent_init(struct ice_agent_t* ice);

struct ice_agent_t* ice_agent_create(int controlling, struct ice_agent_handler_t* handler, void* param)
{
	struct ice_agent_t* ice;
	ice = (struct ice_agent_t*)calloc(1, sizeof(struct ice_agent_t));
	if (ice)
	{
		LIST_INIT_HEAD(&ice->streams);
		ice->param = param;
		ice->nomination = ICE_REGULAR_NOMINATION;
		ice->controlling = controlling;
		ice->tiebreaking = (intptr_t)ice * rand();
		memcpy(&ice->handler, handler, sizeof(ice->handler));
		ice_agent_init(ice);
	}

	return ice;
}

int ice_agent_destroy(struct ice_agent_t* ice)
{
	struct ice_stream_t* s;
	struct list_head *ptr, *next;

	list_for_each_safe(ptr, next, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		ice_stream_destroy(&s);
	}

	stun_agent_destroy(&ice->stun);
	free(ice);
	return 0;
}

int ice_agent_input(struct ice_agent_t* ice, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	return ice && ice->stun ? stun_agent_input(ice->stun, protocol, local, remote, data, bytes) : -1;
}

int ice_agent_send(struct ice_agent_t* ice, uint8_t stream, uint16_t component, const void* data, int bytes)
{
	int i;
	struct ice_stream_t* s;
	struct ice_candidate_t* local, *c;
	struct ice_candidate_t* remote;

	s = ice_agent_find_stream(ice, stream);
	if (NULL == s)
		return -1;

	local = remote = NULL;
	for (i = 0; i < s->ncomponent; i++)
	{
		if (s->components[i].local.component == component)
		{
			local = &s->components[i].local;
			remote = &s->components[i].remote;
			assert(local->stream == stream && remote->stream == stream);
			break;
		}
	}

	for (i = 0; NULL == local && i < ice_candidates_count(&s->locals); i++)
	{
		c = ice_candidates_get(&s->locals, i);
		if (c->component == component)
		{
			local = c;
			break;
		}
	}

	for (i = 0; NULL == remote && i < ice_candidates_count(&s->remotes); i++)
	{
		c = ice_candidates_get(&s->remotes, i);
		if (c->component == component)
		{
			remote = c;
			break;
		}
	}

	if (local && remote)
	{
		if (ICE_CANDIDATE_RELAYED == s->components[i].local.type)
			return turn_agent_send(ice->stun, (struct sockaddr*)&local->addr, (struct sockaddr*)&remote->addr, data, bytes);
		else
			return ice->handler.send(ice->param, STUN_PROTOCOL_UDP, (struct sockaddr*)&local->addr, (struct sockaddr*)&remote->addr, data, bytes);
	}

	return -1;
}

int ice_agent_set_local_auth(struct ice_agent_t* ice, const char* usr, const char* pwd)
{
	memset(&ice->auth, 0, sizeof(ice->auth));
	ice->auth.credential = STUN_CREDENTIAL_SHORT_TERM;
	snprintf(ice->auth.usr, sizeof(ice->auth.usr) , "%s", usr ? usr : "");
	snprintf(ice->auth.pwd, sizeof(ice->auth.pwd), "%s", pwd ? pwd : "");
	return 0;
}

int ice_agent_start(struct ice_agent_t* ice)
{
	struct list_head* ptr, *next;
	struct ice_stream_t* s;
	struct ice_checklist_t *active;

	active = NULL; // default stream
	list_for_each_safe(ptr, next, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		if (ice_candidates_count(&s->locals) < 1 || ice_candidates_count(&s->remotes) < 1)
		{
			assert(0);
			continue;
		}

		s->ncomponent = 0;
		memset(s->components, 0, sizeof(s->components));
		ice_checklist_reset(s->checklist, s, &s->locals, &s->remotes);
		if (NULL == active)
			active = s->checklist;
	}

	if (NULL == active)
		return -1;
	return ice_checklist_start(active, 1);
}

int ice_agent_stop(struct ice_agent_t* ice)
{
	struct list_head *ptr, *next;
	struct ice_stream_t* s;

	list_for_each_safe(ptr, next, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		ice_checklist_cancel(s->checklist);
	}
	return 0;
}

// 7.2. STUN Server Procedures, on bind request
static int ice_agent_onbind(void* param, stun_response_t* resp, const stun_request_t* req)
{
	int r;
	struct ice_agent_t* ice;
	struct ice_stream_t* s;
	struct ice_candidate_t* c;
	struct stun_address_t addr;
	const struct stun_attr_t *nominated, *priority;
	const struct stun_attr_t *controlled, *controlling;

	priority = stun_message_attr_find(&req->msg, STUN_ATTR_PRIORITY);
	nominated = stun_message_attr_find(&req->msg, STUN_ATTR_USE_CANDIDATE);
	controlled = stun_message_attr_find(&req->msg, STUN_ATTR_ICE_CONTROLLED);
	controlling = stun_message_attr_find(&req->msg, STUN_ATTR_ICE_CONTROLLING);
	if (controlled && controlling)
		return 0; // invalid ignore

	memset(&addr, 0, sizeof(addr));
	ice = (struct ice_agent_t*)param;
	r = stun_request_getaddr(req, &addr.protocol, &addr.host, &addr.peer, &addr.reflexive, &addr.relay);
	if (0 != r)
		return r;

	c = ice_agent_find_local_candidate(ice, &addr.host);
	if (!c)
	{
		assert(0);
		return -1; // not found
	}

#if defined(_DEBUG) || defined(DEBUG)
	{
		char ip[256];
		printf("[S] ice [%d:%d] %s%s [%s:%hu] peer: [%s:%hu], relay: [%s:%hu]\n", (int)c->stream, (int)c->component, controlling ? "[controlling]" : "[controlled]", nominated ? "[nominated]" : "", IP(&c->host, ip), PORT(&c->host), IP(&addr.peer, ip + 65), PORT(&addr.peer), IP(&addr.relay, ip + 130), PORT(&addr.relay));
	}
#endif

	// add remote candidates. 
	// However, the agent does not pair this candidate with any local candidates.
	ice_agent_add_remote_peer_reflexive_candidate(ice, c->stream, c->component, &addr, priority);

	s = ice_agent_find_stream(ice, c->stream);
	assert(s);

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
	if ((controlling && ice->controlling && ice->tiebreaking >= controlling->v.u64)
		|| (controlled && 0 == ice->controlling && ice->tiebreaking < controlled->v.u64))
		return stun_agent_bind_response(resp, ICE_ROLE_CONFLICT, "Role Conflict");

	if ((controlling && ice->controlling && ice->tiebreaking < controlling->v.u64)
		|| (controlled && 0 == ice->controlling && ice->tiebreaking >= controlled->v.u64))
	{
		// switch role
		ice->controlling = ice->controlling ? 0 : 1;

		ice_agent_onrolechanged(ice);
	}

	// try trigger check
	ice_checklist_trigger(s->checklist, s, c, &addr, nominated ? 1 : 0);

	// set auth
	memcpy(&resp->auth, &ice->auth, sizeof(resp->auth));
	snprintf(resp->auth.usr, sizeof(resp->auth.usr), "%s:%s", ice->auth.usr, s->rauth.usr);
	return stun_agent_bind_response(resp, 200, "OK");
}

static int ice_agent_onbindindication(void* param, const stun_request_t* req)
{
	struct ice_agent_t* ice;
	ice = (struct ice_agent_t*)param;
	return 0;
}

static int ice_agent_onsend(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	struct ice_agent_t* ice;
	ice = (struct ice_agent_t*)param;
	return ice->handler.send(ice->param, protocol, local, remote, data, bytes);
}

static int ice_agent_auth(void* param, int cred, const char* usr, const char* realm, const char* nonce, char pwd[512])
{
	// rfc5245 7.1.2. Sending the Request
	// A connectivity check MUST utilize the STUN short-term credential mechanism
	char full[512];
	struct ice_agent_t* ice;
	(void)realm, (void)nonce;
	ice = (struct ice_agent_t*)param;
	assert(STUN_CREDENTIAL_SHORT_TERM == ice->stun->auth_term);
	snprintf(full, sizeof(full), "%s:", ice->auth.usr);
	if(0 != strncmp(usr, full, strlen(full)))
		return -1;
	snprintf(pwd, 512, "%s", ice->auth.pwd);
	return 0;
}

static int ice_agent_getnonce(void* param, char realm[128], char nonce[128])
{
	assert(0);
    (void)param;
	realm[0] = nonce[0] = 0;
	return -1;
}

static void ice_agent_ondata(void* param, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	int i;
	struct list_head* ptr;
	struct ice_stream_t* s;
	struct ice_candidate_t* c;
	struct ice_agent_t* ice;

	ice = (struct ice_agent_t*)param;

	if (bytes > 0 && 0 != (0xC0 & ((const uint8_t*)data)[0]))
	{
		list_for_each(ptr, &ice->streams)
		{
			s = list_entry(ptr, struct ice_stream_t, link);
			for (i = 0; i < ice_candidates_count(&s->locals); i++)
			{
				c = ice_candidates_get(&s->locals, i);
				if (ICE_CANDIDATE_HOST ==c->type && 0 == socket_addr_compare(local, (struct sockaddr*)&c->addr))
				{
					if (ice->handler.ondata)
						ice->handler.ondata(ice->param, c->stream, c->component, data, bytes);
					break;
				}
			}			
		}
	}
	else
	{
		ice_agent_input(ice, protocol, local, remote, data, bytes);
	}
}

static int ice_agent_init(struct ice_agent_t* ice)
{
	struct stun_agent_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.auth = ice_agent_auth;
	handler.send = ice_agent_onsend;
	handler.ondata = ice_agent_ondata;
	handler.onbind = ice_agent_onbind; // bind request
	handler.getnonce = ice_agent_getnonce;
	handler.onbindindication = ice_agent_onbindindication;
	ice->stun = stun_agent_create(STUN_RFC_5389, &handler, ice);
	ice->stun->auth_term = STUN_CREDENTIAL_SHORT_TERM;
	return 0;
}
