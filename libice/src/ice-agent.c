#include "ice-agent.h"
#include "ice-internal.h"
#include "stun-agent.h"
#include "stun-internal.h"
#include "ice-streams.h"
#include "ice-checklist.h"
#include "ice-candidates.h"

struct ice_agent_t
{
	stun_agent_t* stun;
	ice_streams_t streams;
	ice_candidate_pairs_t valids; // valid list

	struct stun_credential_t auth; // local auth
	struct ice_agent_handler_t handler;
	void* param;
};

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

static int ice_stun_getnonce(void* param, char realm[128], char nonce[128])
{
	assert(0);
	realm[0] = nonce[0] = 0;
	return -1; (void)param;
}

static int ice_stun_onbind(void* param, stun_response_t* resp, const stun_request_t* req)
{
	int i, r;
	int protocol;
	struct ice_agent_t* ice;
	struct ice_stream_t* stream;
	struct sockaddr_storage local, remote, reflexive;

	ice = (struct ice_agent_t*)param;
	r = stun_request_getaddr(req, &protocol, &local, &remote, &reflexive);
	if (0 != r)
		return r;

	// try trigger check
	for (i = 0; i < ice_streams_count(&ice->streams); i++)
	{
		stream = ice_streams_get(&ice->streams, i);
		ice_checklist_trigger(stream->checks, protocol, &local, &remote);
	}

	return 0;
}

static int ice_stun_onvalid(void* param, struct ice_checklist_t* l, const struct ice_candidate_pair_t* pair)
{
	int i;
	struct ice_agent_t* ice;
	struct ice_stream_t* stream;
	ice = (struct ice_agent_t*)param;

	if (0 != ice_candidate_pairs_insert(&ice->valids, pair))
		return -1;

	if (!ice_checklist_stream_valid(l, &ice->valids))
		return 0;
	
	// sync the other streams
	for (i = 0; i < ice_streams_count(&ice->streams); i++)
	{
		stream = ice_streams_get(&ice->streams, i);
		if (l == stream->checks)
			continue;

		ice_checklist_update(stream->checks, &ice->valids);
	}

	return 0;
}

static int ice_stun_onfinish(void* param, struct ice_checklist_t* l, int code)
{
	int i;
	struct ice_agent_t* ice;
	ice = (struct ice_agent_t*)param;

	for (i = 0; i < ice_streams_count(&ice->streams); i++)
	{
		if (!ice_checklist_stream_valid(l, &ice->valids))
			return 0;
	}

	// all stream finish
	// TODO: callback/notify

	return 0;
}

struct ice_agent_t* ice_create(struct ice_agent_handler_t* handler, void* param)
{
	struct ice_agent_t* ice;
	struct stun_agent_handler_t stun;

	memset(&stun, 0, sizeof(stun));
	stun.send = ice_stun_send;
	stun.auth = ice_stun_auth;
	stun.onbind = ice_stun_onbind;
	stun.getnonce = ice_stun_getnonce;

	ice = (struct ice_agent_t*)calloc(1, sizeof(struct ice_agent_t));
	if (ice)
	{
		ice_streams_init(&ice->streams);
		ice_candidate_pairs_init(&ice->valids);
		ice->stun = stun_agent_create(STUN_RFC_5389, &stun, ice);
		memcpy(&ice->handler, handler, sizeof(ice->handler));
		ice->param = param;
	}
	
	return ice;
}

int ice_destroy(struct ice_agent_t* ice)
{
	if (ice)
	{
		ice_candidate_pairs_free(&ice->valids);
		stun_agent_destroy(&ice->stun);
		ice_streams_free(&ice->streams);
		free(ice);
	}
	return 0;
}

int ice_input(struct ice_agent_t* ice, int protocol, const struct sockaddr* local, const struct sockaddr* remote, const void* data, int bytes)
{
	return ice && ice->stun ? stun_agent_input(ice->stun, protocol, local, remote, data, bytes) : -1;
}

int ice_set_local_auth(struct ice_agent_t* ice, const char* usr, const char* pwd)
{
	memset(&ice->auth, 0, sizeof(ice->auth));
	ice->auth.credential = STUN_CREDENTIAL_SHORT_TERM;
	snprintf(ice->auth.usr, sizeof(ice->auth.usr) , "%s", usr);
	snprintf(ice->auth.pwd, sizeof(ice->auth.pwd), "%s", pwd);
	return 0;
}

int ice_add_local_candidate(struct ice_agent_t* ice, int stream, const struct ice_candidate_t* c)
{
	struct ice_stream_t *s;
	struct ice_checklist_handler_t h;
	s = ice_streams_fetch(&ice->streams, stream);
	if (!s) return -1;

	if (!s->checks)
	{
		memset(&h, 0, sizeof(h));
		h.onvalid = ice_stun_onvalid;
		h.onfinish = ice_stun_onfinish;
		s->checks = ice_checklist_create(ice->stun, &ice->auth, &h, ice);
	}

	return ice_checklist_add_local_candidate(s->checks, c);
}

int ice_add_remote_candidate(struct ice_agent_t* ice, int stream, const struct ice_candidate_t* c)
{
	struct ice_stream_t *s;
	s = ice_streams_fetch(&ice->streams, stream);
	if (!s) return -1;

	return ice_checklist_add_remote_candidate(s->checks, c);
}

int ice_gather_stun_candidate(struct ice_agent_t* ice, ice_agent_ongather ongather, void* param)
{
	int i;
	struct ice_stream_t *s;
	for (i = 0; i < darray_count(&ice->streams); i++)
	{
		s = (struct ice_stream_t *)darray_get(&ice->streams, i);
		if (s && s->checks)
			ice_checklist_gather_stun_candidate(s->checks, ongather, param);
	}

	// TODO:
	return ongather(param, -1);
}

int ice_get_default_candidate(struct ice_agent_t* ice, int stream, ice_component_t component, struct ice_candidate_t* c)
{
	struct ice_stream_t* s;
	
	s = ice_streams_find(&ice->streams, stream);
	if (NULL == s)
		return -1;

	return ice_checklist_get_default_candidate(s->checks, component, c);
}

int ice_list_local_candidate(struct ice_agent_t* ice, int stream, ice_agent_oncandidate oncand, void* param)
{
	struct ice_stream_t* s;

	s = ice_streams_find(&ice->streams, stream);
	if (NULL == s)
		return -1;

	return ice_checklist_list_local_candidate(s->checks, oncand, param);
}

int ice_list_remote_candidate(struct ice_agent_t* ice, int stream, ice_agent_oncandidate oncand, void* param)
{
	struct ice_stream_t* s;

	s = ice_streams_find(&ice->streams, stream);
	if (NULL == s)
		return -1;

	return ice_checklist_list_remote_candidate(s->checks, oncand, param);
}

int ice_start(struct ice_agent_t* ice)
{
	int i;
	struct ice_stream_t* stream;
	
	if (ice_streams_count(&ice->streams) < 1)
		return -1;
	
	for (i = 0; i < ice_streams_count(&ice->streams); i++)
	{
		stream = ice_streams_get(&ice->streams, i);
		if (stream->first)
			break;
	}

	assert(i < ice_streams_count(&ice->streams));
	i = i % ice_streams_count(&ice->streams);
	stream = ice_streams_get(&ice->streams, i);

	return ice_checklist_init(stream->checks);
}
