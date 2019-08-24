#include "ice-internal.h"
#include "ice-candidates.h"
#include "stun-internal.h"
#include <stdlib.h>
#include <string.h>

struct ice_gather_t
{
	struct ice_agent_t* ice;
	ice_candidates_t candidates;
};

static struct ice_gather_t* ice_gather_create(struct ice_agent_t* ice)
{
	struct ice_gather_t* g;
	g = (struct ice_gather_t*)calloc(1, sizeof(*g));
	if (g)
	{
		g->ice = ice;
		ice_candidates_init(&g->candidates);
	}
	return g;
}

static void ice_gather_destroy(struct ice_gather_t* g)
{
	if (g)
	{
		ice_candidates_free(&g->candidates);
		free(g);
	}
}

static int ice_gather_callback(struct ice_gather_t* g)
{
	if (0 != ice_candidates_count(&g->candidates))
		return 0;

	g->ice->handler.ongather(g->ice->param, 0);
	ice_gather_destroy(g);
	return 0;
}

static int ice_gather_onbind(void* param, const stun_request_t* req, int code, const char* phrase)
{
	int r;
	struct ice_gather_t* g;
	struct ice_candidate_t c, *local;
	struct stun_address_t addr;
	
	memset(&addr, 0, sizeof(addr));
	g = (struct ice_gather_t*)param;
	r = stun_request_getaddr(req, &addr.protocol, &addr.host, &addr.peer, &addr.reflexive, &addr.relay);
	local = ice_candidates_find(&g->candidates, ice_candidate_compare_host_addr, &addr.host);
	if (!local)
	{
		assert(0);
		return -1;
	}
	
	if (0 == code)
	{
		assert(AF_INET == addr.reflexive.ss_family || AF_INET6 == addr.reflexive.ss_family);
		memset(&c, 0, sizeof(struct ice_candidate_t));
		c.type = ICE_CANDIDATE_SERVER_REFLEXIVE;
		c.component = local->component;
		c.protocol = local->protocol;
		memcpy(&c.addr, &addr.reflexive, sizeof(c.addr));
		memcpy(&c.host, &addr.host, sizeof(c.host));
		ice_candidate_priority(&c);
		ice_candidate_foundation(&c);
		r = ice_agent_add_local_candidate(g->ice, &c);

		if (AF_INET == addr.relay.ss_family || AF_INET6 == addr.relay.ss_family)
		{
			memset(&c, 0, sizeof(struct ice_candidate_t));
			c.type = ICE_CANDIDATE_RELAYED;
			c.component = local->component;
			c.protocol = local->protocol;
			memcpy(&c.addr, &addr.relay, sizeof(c.addr));
			memcpy(&c.host, &addr.host, sizeof(c.host));
			ice_candidate_priority(&c);
			ice_candidate_foundation(&c);
			r = ice_agent_add_local_candidate(g->ice, &c);
		}
	}
	else
	{
		r = 0; // ignore error
		printf("ice_checklist_ongather code: %d, phrase: %s\n", code, phrase);
	}

	ice_candidates_erase(&g->candidates, local);
	return ice_gather_callback(g);
}

int ice_agent_gather(struct ice_agent_t* ice, const struct sockaddr* addr, int turn, int timeout)
{
	int i, r;
	struct list_head *ptr, *next;
	struct ice_stream_t* s;
	struct ice_gather_t* g;
	struct ice_candidate_t *c;

	if (!addr || (AF_INET != addr->sa_family && AF_INET6 != addr->sa_family))
		return -1;

	g = ice_gather_create(ice);
	if (!g)
		return -1;

	r = 0;
	list_for_each_safe(ptr, next, &ice->streams)
	{
		s = list_entry(ptr, struct ice_stream_t, link);
		for (i = 0; 0 == r && i < ice_candidates_count(&s->locals); i++)
		{
			c = ice_candidates_get(&s->locals, i);
			if (ICE_CANDIDATE_HOST != c->type)
				continue;

			r = ice_candidates_insert(&g->candidates, c);
			assert(0 == r);
		}
	}
	if (0 != r)
		return r;

	for (i = 0; i < ice_candidates_count(&g->candidates); i++)
	{
		c = ice_candidates_get(&g->candidates, i);
		if (0 == turn)
			r = ice_agent_bind(ice, (const struct sockaddr*)&c->host, addr, NULL, timeout, ice_gather_onbind, g);
		else
			r = ice_agent_allocate(ice, (const struct sockaddr*)&c->host, addr, NULL, timeout, ice_gather_onbind, g);
		if (0 != r)
		{
			// TODO: notify
			ice_candidates_erase(&g->candidates, c);
			i--; // for erase
		}
	}

	return ice_gather_callback(g);
}
