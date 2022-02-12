#ifndef _ice_candidates_h_
#define _ice_candidates_h_

#include "ice-internal.h"

static inline void ice_candidates_init(ice_candidates_t* arr)
{
	memset(arr, 0, sizeof(*arr));
	darray_init(arr, sizeof(struct ice_candidate_t), 8);
}

static inline void ice_candidates_free(ice_candidates_t* arr)
{
	darray_free(arr);
}

static inline void ice_candidates_clear(ice_candidates_t* arr)
{
	darray_clear(arr);
}

static inline int ice_candidates_count(const ice_candidates_t* arr)
{
	return darray_count(arr);
}

static inline struct ice_candidate_t* ice_candidates_get(ice_candidates_t* arr, int i)
{
	return (struct ice_candidate_t*)darray_get(arr, i);
}

/// Compare candidate host address with addr
/// @return 0-equal
static inline int ice_candidate_compare_host_addr(const struct ice_candidate_t* l, const struct sockaddr_storage* addr)
{
	return ICE_CANDIDATE_HOST == l->type && 0 == socket_addr_compare((const struct sockaddr*)&l->host, (const struct sockaddr*)addr) ? 0 : -1;
}

static inline int ice_candidate_compare_addr(const struct ice_candidate_t* l, const struct sockaddr_storage* addr)
{
	return 0 == socket_addr_compare((const struct sockaddr*)&l->addr, (const struct sockaddr*)addr) ? 0 : -1;
}

static inline int ice_candidate_compare_base_addr(const struct ice_candidate_t* l, const struct ice_candidate_t* r)
{
	// rfc5245 B.2. Candidates with Multiple Bases (p109)
	return l->type == r->type && 0 == socket_addr_compare((const struct sockaddr*)ice_candidate_base(l), (const struct sockaddr*)ice_candidate_base(r)) ? 0 : -1;
}

static inline int ice_candidate_compare(const struct ice_candidate_t* l, const struct ice_candidate_t* r)
{
	// multi-home maybe has same reflexive address
	// eth0: 192.168.10.100 --> stun server 1
	// eth1: 192.168.1.2 --> 10.2.2.10 --> 192.168.10.100 --> stun server 2
	return l->stream == r->stream && l->component == r->component /*&& l->type == r->type*/
		&& (0 == socket_addr_compare((const struct sockaddr*)&l->addr, (const struct sockaddr*)&r->addr)
		&& 0 == socket_addr_compare((const struct sockaddr*)ice_candidate_base(l), (const struct sockaddr*)ice_candidate_base(r))) ? 0 : -1;
}

static inline int ice_candidates_insert(ice_candidates_t* arr, const struct ice_candidate_t* c)
{
	return darray_insert2(arr, c, (darray_compare)ice_candidate_compare);
}

static inline int ice_candidates_erase(ice_candidates_t* arr, const struct ice_candidate_t* c)
{
	return darray_erase2(arr, c, (darray_compare)ice_candidate_compare);
}

static inline int ice_candidates_list(ice_candidates_t* arr, int (*oncandidate)(const struct ice_candidate_t*, void*), void* param)
{
	int i, r;
	struct ice_candidate_t* c;
	for (i = 0; i < darray_count(arr); i++)
	{
		c = ice_candidates_get(arr, i);
		r = oncandidate(c, param);
		if (0 != r)
			return r;
	}
	return 0;
}

static inline struct ice_candidate_t* ice_candidates_find(ice_candidates_t* arr, int (*oncandidate)(const struct ice_candidate_t*, const struct sockaddr_storage*), const void* param)
{
	return (struct ice_candidate_t*)darray_find(arr, param, NULL, (darray_compare)oncandidate);
}

//////////////////////////////////////////////////////////////////////////

static inline void ice_candidate_pairs_init(ice_candidate_pairs_t* arr)
{
	memset(arr, 0, sizeof(*arr));
	darray_init(arr, sizeof(struct ice_candidate_pair_t), 16);
}

static inline void ice_candidate_pairs_free(ice_candidate_pairs_t* arr)
{
	darray_free(arr);
}

static inline void ice_candidate_pairs_clear(ice_candidate_pairs_t* arr)
{
	darray_clear(arr);
}

static inline int ice_candidate_pairs_count(const ice_candidate_pairs_t* arr)
{
	return darray_count(arr);
}

static inline struct ice_candidate_pair_t* ice_candidate_pairs_get(ice_candidate_pairs_t* arr, int i)
{
	return (struct ice_candidate_pair_t*)darray_get(arr, i);
}

static inline int ice_candidate_pair_compare_foundation(const struct ice_candidate_pair_t** l, const struct ice_candidate_pair_t* r)
{
	return 0 == strcmp((*l)->foundation, r->foundation) ? 0 : -1;
}

static inline int ice_candidate_pair_compare(const struct ice_candidate_pair_t* l, const struct ice_candidate_pair_t* r)
{
	if (l->priority == r->priority)
	{
		if (0 != memcmp(&l->local, &r->local, sizeof(struct ice_candidate_t)))
			return memcmp(&l->local, &r->local, sizeof(struct ice_candidate_t));
		if (0 != memcmp(&l->remote, &r->remote, sizeof(struct ice_candidate_t)))
			return memcmp(&l->remote, &r->remote, sizeof(struct ice_candidate_t));
		return 0;
	}
	return (l->priority - r->priority) > 0 ? 1 : -1;
}

static inline int ice_candidate_pair_compare_addr(const struct ice_candidate_pair_t* pair, const struct stun_address_t* addr)
{
	if (0 == socket_addr_compare((const struct sockaddr*) & pair->local.host, (const struct sockaddr*) & addr->host)
		&& 0 == socket_addr_compare((const struct sockaddr*) & pair->remote.host, (const struct sockaddr*) & addr->peer)
		&& (ICE_CANDIDATE_RELAYED != pair->local.type || 0 == socket_addr_compare((const struct sockaddr*) & pair->local.addr, (const struct sockaddr*)&addr->relay)))
	{
		return 0;
	}
	return -1;
}

static inline struct ice_candidate_pair_t* ice_candidate_pairs_find(ice_candidate_pairs_t* arr, int (*onpair)(const struct ice_candidate_pair_t*, const struct stun_address_t*), const void* param)
{
	return (struct ice_candidate_pair_t*)darray_find(arr, param, NULL, (darray_compare)onpair);
}

static inline int ice_candidate_pairs_insert(ice_candidate_pairs_t* arr, const struct ice_candidate_pair_t* pair)
{
	return darray_insert2(arr, pair, (darray_compare)ice_candidate_pair_compare);
}

static inline int ice_candidate_pairs_remove(ice_candidate_pairs_t* arr, const struct ice_candidate_pair_t* pair)
{
	return darray_erase2(arr, pair, (darray_compare)ice_candidate_pair_compare);
}

//////////////////////////////////////////////////////////////////////////
typedef struct darray_t ice_candidate_components_t;
typedef struct ice_candidate_component_t
{
	uint16_t id;
	ice_candidate_pairs_t component;
} ice_candidate_component_t;

static inline void ice_candidate_components_init(ice_candidate_components_t* components)
{
	memset(components, 0, sizeof(*components));
	darray_init(components, sizeof(ice_candidate_component_t), 2); // RTP/RTCP
}

static inline void ice_candidate_components_free(ice_candidate_components_t* components)
{
	int i;
	ice_candidate_component_t* component;
	for (i = 0; i < darray_count(components); i++)
	{
		component = (ice_candidate_component_t*)darray_get(components, i);
		ice_candidate_pairs_free(&component->component);
	}
	darray_free(components);
}

static inline void ice_candidate_components_clear(ice_candidate_components_t* components)
{
	int i;
	ice_candidate_component_t* component;
	for (i = 0; i < darray_count(components); i++)
	{
		component = (ice_candidate_component_t*)darray_get(components, i);
		darray_clear(&component->component);
	}
	darray_clear(components); // reset pairs size
}

static inline int ice_candidate_components_count(ice_candidate_components_t* components)
{
	return darray_count(components);
}

static inline ice_candidate_component_t* ice_candidate_components_get(ice_candidate_components_t* components, int i)
{
	return (ice_candidate_component_t*)darray_get(components, i);
}

static inline struct ice_candidate_pair_t* ice_candidate_components_find(ice_candidate_components_t* components, const struct stun_address_t* addr)
{
	int i;
	struct ice_candidate_pair_t *pair;
	ice_candidate_component_t* component;

	pair = NULL;
	for (i = 0; i < ice_candidate_components_count(components) && NULL == pair; i++)
	{
		component = ice_candidate_components_get(components, i);
		pair = ice_candidate_pairs_find(&component->component, ice_candidate_pair_compare_addr, addr);
	}
	return pair;
}

static inline int ice_candidate_component_compare(const ice_candidate_component_t* component, const uint16_t *id)
{
	return (int)(component->id - *id);
}

static ice_candidate_component_t* ice_candidate_components_fetch(ice_candidate_components_t* components, uint16_t id)
{
	int pos;
	ice_candidate_component_t arr, *component;
	component = darray_find(components, &id, &pos, (darray_compare)ice_candidate_component_compare);
	if (NULL == component)
	{
		memset(&arr, 0, sizeof(arr));
		arr.id = id;
		darray_init(&arr.component, sizeof(struct ice_candidate_pair_t), 9);
		if (0 != darray_insert(components, pos, &arr))
			return NULL;
		component = (ice_candidate_component_t*)darray_get(components, pos);
	}
	return component;
}

#endif /* !_ice_candidates_h_ */
