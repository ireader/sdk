#include "ice-internal.h"
#include "ice-candidates.h"
#include "stun-internal.h"
#include <stdlib.h>
#include <string.h>

struct ice_gather_t
{
	struct ice_agent_t* ice;
	ice_candidates_t candidates;
	ice_agent_ongather ongather;
	void* param;
};

static struct ice_gather_t* ice_gather_create(struct ice_agent_t* ice, ice_agent_ongather ongather, void* param)
{
	struct ice_gather_t* g;
	g = (struct ice_gather_t*)calloc(1, sizeof(*g));
	if (g)
	{
		g->ice = ice;
		ice_agent_addref(ice);

		ice_candidates_init(&g->candidates);
		g->ongather = ongather;
		g->param = param;
	}
	return g;
}

static void ice_gather_destroy(struct ice_gather_t* g)
{
	if (g)
	{
		ice_candidates_free(&g->candidates);
		ice_agent_release(g->ice);
		free(g);
	}
}

static int ice_gather_callback(struct ice_gather_t* g)
{
	int r;
	if (0 != ice_candidates_count(&g->candidates))
		return 0;

	r = g->ongather(g->param, 0);
	ice_gather_destroy(g);
	return r;
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
		memcpy(&c.stun, &addr.peer, sizeof(c.stun));
		memcpy(&c.host, &addr.host, sizeof(c.host));
		//memcpy(&c.relay, &addr.relay, sizeof(c.relay));
		memcpy(&c.reflexive, &addr.reflexive, sizeof(c.reflexive));
		ice_candidate_priority(&c);
		ice_candidate_foundation(&c);
		r = ice_add_local_candidate(g->ice, &c);

		if (AF_INET == addr.relay.ss_family || AF_INET6 == addr.relay.ss_family)
		{
			memset(&c, 0, sizeof(struct ice_candidate_t));
			c.type = ICE_CANDIDATE_RELAYED;
			c.component = local->component;
			c.protocol = local->protocol;
			memcpy(&c.stun, &addr.peer, sizeof(c.stun));
			memcpy(&c.host, &addr.host, sizeof(c.host));
			memcpy(&c.relay, &addr.relay, sizeof(c.relay));
			memcpy(&c.reflexive, &addr.reflexive, sizeof(c.reflexive));
			ice_candidate_priority(&c);
			ice_candidate_foundation(&c);
			r = ice_add_local_candidate(g->ice, &c);
		}
	}
	else
	{
		r = 0; // ignore g error
		printf("ice_checklist_ongather code: %d, phrase: %s\n", code, phrase);
	}

	ice_candidates_erase(&g->candidates, local);
	return ice_gather_callback(g);
}

int ice_gather_candidate(struct ice_agent_t* ice, const struct sockaddr* addr, int turn, ice_agent_ongather ongather, void* param)
{
	int i, r;
	struct ice_gather_t* g;
	struct ice_candidate_t *c;

	if (!addr || (AF_INET != addr->sa_family && AF_INET6 != addr->sa_family))
		return -1;

	g = ice_gather_create(ice, ongather, param);
	if (!g)
		return -1;

	locker_lock(&ice->locker);
	for (i = 0; i < ice_candidates_count(&ice->locals); i++)
	{
		c = ice_candidates_get(&ice->locals, i);
		if (ICE_CANDIDATE_HOST != c->type)
			continue;

		r = ice_candidates_insert(&g->candidates, c);
		assert(0 == r);
	}
	locker_unlock(&ice->locker);

	for (i = 0; i < ice_candidates_count(&g->candidates); i++)
	{
		c = ice_candidates_get(&g->candidates, i);
		if (0 == turn)
			r = ice_agent_bind(ice, (const struct sockaddr*)&c->host, addr, NULL, ice_gather_onbind, g);
		else
			r = ice_agent_allocate(ice, (const struct sockaddr*)&c->host, addr, NULL, ice_gather_onbind, g);

		if (0 != r)
		{
			// TODO: notify
			ice_candidates_erase(&g->candidates, c);
		}
	}

	return ice_gather_callback(g);
}
