#include "ice-checklist.h"
#include "ice-candidates.h"
#include "stun-internal.h"

static int ice_gather_onbind(void* param, const stun_request_t* req, int code, const char* phrase)
{
	int i, r, protocol;
	struct ice_checklist_t* l;
	struct ice_candidate_t c, *local;

	l = (struct ice_checklist_t*)param;

	if (code >= 200 && code < 300)
	{
		memset(&c, 0, sizeof(struct ice_candidate_t));
		c.type = ICE_CANDIDATE_SERVER_REFLEXIVE;
		stun_request_getaddr(req, &protocol, &c.base, &c.stun, &c.addr);
		ice_candidate_priority(&c);
		ice_candidate_foundation(&c);

		for (i = 0; i < ice_candidates_count(&l->locals); i++)
		{
			local = ice_candidates_get(&l->locals, i);
			if (0 == socket_addr_compare((const struct sockaddr*)&local->addr, (const struct sockaddr*)&c.base) && 0 == socket_addr_compare((const struct sockaddr*)&local->stun, (const struct sockaddr*)&c.stun))
			{
				c.componentId = local->componentId;
				break;
			}
		}

		r = ice_checklist_add_local_candidate(l, &c);
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
int ice_checklist_gather_stun_candidate(struct ice_checklist_t* l, ice_agent_ongather ongather, void* param)
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
		if (p->type != ICE_CANDIDATE_HOST || 0 == p->stun.ss_family)
			continue;

		req = stun_request_create(l->stun, STUN_RFC_5389, ice_gather_onbind, l);
		if (!req) continue;

		stun_request_setaddr(req, STUN_PROTOCOL_UDP, &p->addr, &p->stun);
		stun_request_setauth(req, l->auth->credential, l->auth->usr, l->auth->pwd, l->auth->realm, l->auth->nonce);
		r = stun_agent_bind(req);
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
