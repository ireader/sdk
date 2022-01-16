#include "ice-internal.h"
#include "ice-checklist.h"
#include "ice-candidates.h"

int ice_agent_onrolechanged(void* param)
{
	struct list_head* ptr;
	struct ice_stream_t* s;
	struct ice_agent_t* ice;

	ice = (struct ice_agent_t*)param;
	list_for_each(ptr, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		ice_checklist_onrolechanged(s->checklist, ice->controlling);
	}
	return 0;
}

static int ice_stream_onbind(void* param, struct ice_checklist_t* l, const struct darray_t* valids)
{
	struct list_head* ptr;
	struct ice_stream_t* s;
	struct ice_agent_t* ice;

	ice = (struct ice_agent_t*)param;
	list_for_each(ptr, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		if (s->checklist == l)
		{
			ice_checklist_conclude(l);
			continue;
		}

		// sync the other streams
		ice_checklist_update(s->checklist, valids);
	}
	return 0;
}

static int ice_stream_oncomplete(void* param, struct ice_checklist_t* l, int failed)
{
	int status;
	uint64_t mask, flags;
	struct list_head* ptr;
	struct ice_stream_t* s;
	struct ice_agent_t* ice;

	mask = flags = 0;
	assert(0 == failed || 1 == failed);
	ice = (struct ice_agent_t*)param;
	list_for_each(ptr, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		if (s->checklist == l)
			status = failed ? ICE_CHECKLIST_FAILED : ICE_CHECKLIST_COMPLETED;
		else
			status = ice_checklist_getstatus(s->checklist);

		mask |= ((uint64_t)1) << s->stream;
		switch (status)
		{
		case ICE_CHECKLIST_FAILED:
			++failed;
			break; // TODO ???
		case ICE_CHECKLIST_COMPLETED:
			assert(s->stream < 64);
			flags |= ((uint64_t)1) << s->stream;
			s->ncomponent = ice_checklist_getnominated(s->checklist, s->components, sizeof(s->components)/sizeof(s->components[0]));
			break;
		default:
			return 0; // wait for other stream
		}
	}

	// TODO: bind relay channel

	// TODO: call once time
	ice->handler.onconnected(ice->param, flags, mask);
	return 0;
}

static struct ice_stream_t* ice_stream_create(struct ice_agent_t* ice, int stream)
{
	struct ice_stream_t* s;
	struct ice_checklist_handler_t h;

	s = (struct ice_stream_t*)calloc(1, sizeof(struct ice_stream_t));
	if (s)
	{
		s->stream = stream;
		s->status = ICE_CHECKLIST_FROZEN;
		LIST_INIT_HEAD(&s->link);
		ice_candidates_init(&s->locals);
		ice_candidates_init(&s->remotes);

		memset(&h, 0, sizeof(h));
		h.onbind = ice_stream_onbind;
		h.oncomplete = ice_stream_oncomplete;
		h.onrolechanged = ice_agent_onrolechanged;
		s->checklist = ice_checklist_create(ice, &h, ice);
	}
	return s;
}

int ice_stream_destroy(struct ice_stream_t** pp)
{
	struct ice_stream_t* s;
	if (!pp || !*pp)
		return 0;

	s = *pp;
	*pp = NULL;
	ice_candidates_free(&s->locals);
	ice_candidates_free(&s->remotes);
	ice_checklist_destroy(&s->checklist);
	return 0;
}

struct ice_candidate_t* ice_agent_find_local_candidate(struct ice_agent_t* ice, const struct sockaddr_storage* host)
{
	struct list_head* ptr;
	struct ice_stream_t* s;
	struct ice_candidate_t* c;
	list_for_each(ptr, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		c = ice_candidates_find(&s->locals, ice_candidate_compare_host_addr, host);
		if (NULL != c)
			return c;
	}
	return NULL;
}

struct ice_candidate_t* ice_agent_find_remote_candidate(struct ice_agent_t* ice, const struct sockaddr_storage* addr)
{
	struct list_head* ptr;
	struct ice_stream_t* s;
	struct ice_candidate_t* c;
	list_for_each(ptr, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		c = ice_candidates_find(&s->remotes, ice_candidate_compare_addr, addr);
		if (NULL != c)
			return c;
	}
	return NULL;
}

int ice_agent_active_checklist_count(struct ice_agent_t* ice)
{
	int n;
	struct list_head* ptr;
	struct ice_stream_t* s;

	n = 0;
	list_for_each(ptr, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		if (ICE_CHECKLIST_FROZEN != ice_checklist_getstatus(s->checklist))
		{
			n++;
		}
	}
	return n;
}

struct ice_stream_t* ice_agent_find_stream(struct ice_agent_t* ice, int stream)
{
	struct list_head* ptr;
	struct ice_stream_t* s;

	list_for_each(ptr, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		if (s->stream == stream)
			return s;
	}
	return NULL;
}

static struct ice_stream_t* ice_agent_fetch_stream(struct ice_agent_t* ice, int stream)
{
	struct ice_stream_t* s;
	s = ice_agent_find_stream(ice, stream);
	if (NULL == s)
	{
		s = ice_stream_create(ice, stream);
		if (s)
			list_insert_after(&s->link, ice->streams.prev);
	}
	return s;
}


int ice_agent_set_remote_auth(struct ice_agent_t* ice, int stream, const char* usr, const char* pwd)
{
	struct ice_stream_t* s;
	s = ice_agent_fetch_stream(ice, stream);
	if (NULL == s)
		return -1;

	memset(&s->rauth, 0, sizeof(s->rauth));
	s->rauth.credential = STUN_CREDENTIAL_SHORT_TERM;
	snprintf(s->rauth.usr, sizeof(s->rauth.usr), "%s", usr ? usr : "");
	snprintf(s->rauth.pwd, sizeof(s->rauth.pwd), "%s", pwd ? pwd : "");
	return 0;
}

int ice_agent_add_local_candidate(struct ice_agent_t* ice, const struct ice_candidate_t* cand)
{
	struct ice_stream_t* s;
	if (!cand || 0 == cand->priority || 0 == cand->foundation[0] || cand->component < 1 || cand->component > 256
		|| (ICE_CANDIDATE_HOST != cand->type && ICE_CANDIDATE_SERVER_REFLEXIVE != cand->type && ICE_CANDIDATE_RELAYED != cand->type && ICE_CANDIDATE_PEER_REFLEXIVE != cand->type)
		|| (STUN_PROTOCOL_UDP != cand->protocol && STUN_PROTOCOL_TCP != cand->protocol && STUN_PROTOCOL_TLS != cand->protocol && STUN_PROTOCOL_DTLS != cand->protocol)
		|| (AF_INET != cand->addr.ss_family && AF_INET6 != cand->addr.ss_family) 
		|| (AF_INET != cand->host.ss_family && AF_INET6 != cand->host.ss_family)
		|| cand->stream >= 64) // for onconnected callback stream bitmask
	{
		assert(0);
		return -1;
	}

	s = ice_agent_fetch_stream(ice, cand->stream);
	if (NULL == s)
		return -1;

	assert(ice_candidates_count(&s->locals) <= ICE_CANDIDATE_LIMIT);
	if (ice_candidates_count(&s->locals) > ICE_CANDIDATE_LIMIT)
		return -1;
	return ice_candidates_insert(&s->locals, cand);
}

int ice_agent_add_remote_candidate(struct ice_agent_t* ice, const struct ice_candidate_t* cand)
{
	struct ice_stream_t* s;
	if (!cand || 0 == cand->priority || /*0 == cand->foundation[0] || */ cand->component < 1 || cand->component > 256
		|| (ICE_CANDIDATE_HOST != cand->type && ICE_CANDIDATE_SERVER_REFLEXIVE != cand->type && ICE_CANDIDATE_RELAYED != cand->type && ICE_CANDIDATE_PEER_REFLEXIVE != cand->type)
		|| (STUN_PROTOCOL_UDP != cand->protocol && STUN_PROTOCOL_TCP != cand->protocol && STUN_PROTOCOL_TLS != cand->protocol && STUN_PROTOCOL_DTLS != cand->protocol)
		|| (AF_INET != cand->addr.ss_family && AF_INET6 != cand->addr.ss_family)
		|| (AF_INET != cand->host.ss_family && AF_INET6 != cand->host.ss_family)
		|| cand->stream >= 64) // for onconnected callback stream bitmask
	{
		assert(0);
		return -1;
	}

	s = ice_agent_fetch_stream(ice, cand->stream);
	if (NULL == s)
		return -1;

	assert(ice_candidates_count(&s->remotes) <= ICE_CANDIDATE_LIMIT);
	if (ice_candidates_count(&s->remotes) > ICE_CANDIDATE_LIMIT)
		return -1;
	return ice_candidates_insert(&s->remotes, cand);
}

int ice_agent_list_local_candidate(struct ice_agent_t* ice, ice_agent_oncandidate oncand, void* param)
{
	int r;
	struct list_head* ptr;
	struct ice_stream_t* s;
	list_for_each(ptr, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		r = ice_candidates_list(&s->locals, oncand, param);
		if (0 != r)
			return r;
	}
	return 0;
}

int ice_agent_list_remote_candidate(struct ice_agent_t* ice, ice_agent_oncandidate oncand, void* param)
{
	int r;
	struct list_head* ptr;
	struct ice_stream_t* s;
	list_for_each(ptr, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		r = ice_candidates_list(&s->remotes, oncand, param);
		if (0 != r)
			return r;
	}
	return 0;
}

int ice_agent_get_local_candidate(struct ice_agent_t* ice, uint8_t stream, uint16_t component, struct ice_candidate_t* c)
{
	int i;
	struct ice_stream_t* s;
	struct ice_candidate_t *p, *pc = NULL;

	s = ice_agent_find_stream(ice, stream);
	if (NULL == s)
		return -1;

	// 1. choose nominated candidate if we have
	for (i = 0; i < s->ncomponent; i++)
	{
		assert(s->components[i].local.stream == stream);
		if (s->components[i].local.component == component)
		{
			memcpy(c, &s->components[i].local, sizeof(struct ice_candidate_t));
			return 0;
		}
	}

	// 2. choose default candidate
	for (i = 0; i < ice_candidates_count(&s->locals); i++)
	{
		p = ice_candidates_get(&s->locals, i);
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

int ice_agent_get_remote_candidate(struct ice_agent_t* ice, uint8_t stream, uint16_t component, struct ice_candidate_t* c)
{
	int i;
	struct ice_stream_t* s;
	struct ice_candidate_t *p, *pc = NULL;

	s = ice_agent_find_stream(ice, stream);
	if (NULL == s)
		return -1;

	// 1. choose nominated candidate if we have
	for (i = 0; i < s->ncomponent; i++)
	{
		assert(s->components[i].local.stream == stream);
		if (s->components[i].local.component == component)
		{
			memcpy(c, &s->components[i].remote, sizeof(struct ice_candidate_t));
			return 0;
		}
	}

	// 2. choose default candidate
	for (i = 0; i < ice_candidates_count(&s->remotes); i++)
	{
		p = ice_candidates_get(&s->remotes, i);
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
