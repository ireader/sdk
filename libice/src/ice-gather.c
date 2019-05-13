#include "ice-checklist.h"
#include "ice-candidates.h"
#include "stun-internal.h"

static int ice_gather_onbind(void* param, const stun_request_t* req, int code, const char* phrase)
{
	int i, r, protocol;
	struct ice_checklist_t* l;
	struct ice_candidate_t c, *local;
	struct sockaddr_storage host, remote, reflexive, relay;

	l = (struct ice_checklist_t*)param;

	if (code >= 200 && code < 300)
	{
		stun_request_getaddr(req, &protocol, &host, &remote, &reflexive, &relay);

		for (i = 0; i < ice_candidates_count(&l->locals); i++)
		{
			local = ice_candidates_get(&l->locals, i);
			if (ICE_CANDIDATE_HOST == local->type && 0 == socket_addr_compare((const struct sockaddr*)&local->addr, (const struct sockaddr*)&host))
				break;
		}
		
		if (i < ice_candidates_count(&l->locals))
		{
			assert(AF_INET == reflexive.ss_family || AF_INET6 == reflexive.ss_family);
			memset(&c, 0, sizeof(struct ice_candidate_t));
			c.type = ICE_CANDIDATE_SERVER_REFLEXIVE;
			c.component = local->component;
			c.protocol = local->protocol;
			memcpy(&c.raddr, &host, sizeof(c.raddr));
			memcpy(&c.addr, &reflexive, sizeof(c.addr));
			ice_candidate_priority(&c);
			ice_candidate_foundation(&c, (struct sockaddr*)&remote);
			r = ice_checklist_add_local_candidate(l, &c);

			if (AF_INET == relay.ss_family || AF_INET6 == relay.ss_family)
			{
				memset(&c, 0, sizeof(struct ice_candidate_t));
				c.type = ICE_CANDIDATE_RELAYED;
				c.component = local->component;
				c.protocol = local->protocol;
				memcpy(&c.raddr, &host, sizeof(c.raddr));
				memcpy(&c.addr, &reflexive, sizeof(c.addr));
				ice_candidate_priority(&c);
				ice_candidate_foundation(&c, (struct sockaddr*)&remote);
				r = ice_checklist_add_local_candidate(l, &c);
			}
		}
		else
		{
			assert(0);
		}
	}
	else
	{
		r = 0; // ignore gather error
		printf("ice_checklist_ongather code: %d, phrase: %s\n", code, phrase);
	}

	darray_erase2(&l->gathers, req, NULL);
	if (0 == darray_count(&l->gathers))
		return l->ongather(l->ongatherparam, 0);
	return 0;
}

/// Gather server reflexive and relayed candidates
int ice_checklist_gather_stun_candidate(struct ice_checklist_t* l, const struct sockaddr* addr, int turn, ice_agent_ongather ongather, void* param, turn_agent_ondata ondata, void* ondataparam)
{
	int i, r;
	struct stun_request_t* req;
	struct ice_candidate_t *p;
	
	l->gathers.count = 0; // reset gather array
	l->ongatherparam = param;
	l->ongather = ongather;

	for (i = 0; i < ice_candidates_count(&l->locals); i++)
	{
		p = ice_candidates_get(&l->locals, i);
		if (ICE_CANDIDATE_HOST != p->type)
			continue;

		req = stun_request_create(l->stun, STUN_RFC_5389, ice_gather_onbind, l);
		if (!req) continue;

		stun_request_setaddr(req, STUN_PROTOCOL_UDP, (const struct sockaddr*)&p->addr, addr);
		stun_request_setauth(req, l->auth->credential, l->auth->usr, l->auth->pwd, l->auth->realm, l->auth->nonce);
		r = 0 == turn ? stun_agent_bind(req) : turn_agent_allocate(req, ondata, ondataparam);
		if (0 != r)
		{
			stun_request_destroy(&req);
			continue;
		}

		darray_push_back(&l->gathers, &req, 1);
	}

	// empty?
	if (0 == darray_count(&l->gathers))
		return l->ongather(l->ongatherparam, 0);
	return 0;
}
