#ifndef _ice_candidates_h_
#define _ice_candidates_h_

#include "darray.h"

typedef struct darray_t ice_candidates_t;

static inline void ice_candidates_init(ice_candidates_t* arr)
{
	darray_init(arr, sizeof(struct ice_candidate_t), 8);
}

static inline void ice_candidates_free(ice_candidates_t* arr)
{
	darray_free(arr);
}

static inline int ice_candidates_count(ice_candidates_t* arr)
{
	return darray_count(arr);
}

static inline struct ice_candidate_t* ice_candidates_get(ice_candidates_t* arr, int i)
{
	return (struct ice_candidate_t*)darray_get(arr, i);
}

static int ice_candidate_compare_addr(const struct ice_candidate_t* l, const struct sockaddr_storage* addr)
{
	return 0 == socket_addr_compare((const struct sockaddr*)&l->addr, (const struct sockaddr*)addr) ? 0 : -1;
}

static int ice_candidate_compare_host_addr(const struct ice_candidate_t* l, const struct sockaddr_storage* addr)
{
	return ICE_CANDIDATE_HOST == l->type && 0 == socket_addr_compare((const struct sockaddr*)&l->addr, (const struct sockaddr*)addr) ? 0 : -1;
}

static int ice_candidate_compare(const struct ice_candidate_t* l, const struct ice_candidate_t* r)
{
	// rfc5245 B.2. Candidates with Multiple Bases (p109)
	return l->type == r->type && 0 == socket_addr_compare((const struct sockaddr*)&l->addr, (const struct sockaddr*)&r->addr) ? 0 : -1;
}

static inline int ice_candidates_insert(ice_candidates_t* arr, const struct ice_candidate_t* c)
{
	return darray_insert2(arr, c, ice_candidate_compare);
}

static inline int ice_candidates_erase(ice_candidates_t* arr, const struct ice_candidate_t* c)
{
	return darray_erase2(arr, c, ice_candidate_compare);
}

static inline int ice_candidates_list(ice_candidates_t* arr, int (*oncandidate)(void*, const struct ice_candidate_t*), void* param)
{
	int i, r;
	struct ice_candidate_t* c;
	for (i = 0; i < darray_count(arr); i++)
	{
		c = ice_candidates_get(arr, i);
		r = oncandidate(param, c);
		if (0 != r)
			return r;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
typedef struct darray_t ice_candidate_pairs_t;

static inline void ice_candidate_pairs_init(ice_candidate_pairs_t* arr)
{
	darray_init(arr, sizeof(struct ice_candidate_pair_t), 16);
}

static inline void ice_candidate_pairs_free(ice_candidate_pairs_t* arr)
{
	darray_free(arr);
}

static inline int ice_candidate_pairs_count(const ice_candidate_pairs_t* arr)
{
	return darray_count(arr);
}

static inline struct ice_candidate_pair_t* ice_candidate_pairs_get(ice_candidate_pairs_t* arr, int i)
{
	return (struct ice_candidate_pair_t*)darray_get(arr, i);
}

static int ice_candidate_pair_compare_foundation(const struct ice_candidate_pair_t* l, const struct ice_candidate_pair_t* r)
{
	return 0 == strcmp(l->foundation, r->foundation) ? 0 : -1;
}

static int ice_candidate_pair_compare(const struct ice_candidate_pair_t* l, const struct ice_candidate_pair_t* r)
{
	if (l->priority == r->priority)
	{
		if (0 != memcmp(&l->local, &r->local, sizeof(struct ice_candidate_t)))
			return memcmp(&l->local, &r->local, sizeof(struct ice_candidate_t));
		if (0 != memcmp(&l->remote, &r->remote, sizeof(struct ice_candidate_t)))
			return memcmp(&l->remote, &r->remote, sizeof(struct ice_candidate_t));
		return 0;
	}
	return (int)(l->priority - r->priority);
}

static inline int ice_candidate_pairs_insert(ice_candidate_pairs_t* arr, const struct ice_candidate_pair_t* pair)
{
	return darray_insert2(arr, pair, ice_candidate_pair_compare);
}

static inline int ice_candidate_pairs_remove(ice_candidate_pairs_t* arr, const struct ice_candidate_pair_t* pair)
{
	return darray_erase2(arr, pair, ice_candidate_pair_compare);
}

//////////////////////////////////////////////////////////////////////////
typedef struct darray_t ice_candidate_components_t;

static inline void ice_candidate_components_init(ice_candidate_components_t* components)
{
	darray_init(components, sizeof(ice_candidate_pairs_t), 2); // RTP/RTCP
}

static inline void ice_candidate_components_free(ice_candidate_components_t* components)
{
	int i;
	ice_candidate_pairs_t* component;
	for (i = 0; i < darray_count(components); i++)
	{
		component = (ice_candidate_pairs_t*)darray_get(components, i);
		ice_candidate_pairs_free(component);
	}
	darray_free(components);
}

static inline void ice_candidate_components_reset(ice_candidate_components_t* components)
{
	int i;
	ice_candidate_pairs_t* component;
	for (i = 0; i < darray_count(components); i++)
	{
		component = (ice_candidate_pairs_t*)darray_get(components, i);
		component->count = 0;
	}
	components->count = 0; // reset pairs size
}

static inline int ice_candidate_components_count(ice_candidate_components_t* components)
{
	return darray_count(components);
}

static inline ice_candidate_pairs_t* ice_candidate_components_get(ice_candidate_components_t* components, int i)
{
	return (ice_candidate_pairs_t*)darray_get(components, i);
}

static int ice_candidate_component_compare(const ice_candidate_pairs_t* component, const uint16_t *id)
{
	const struct ice_candidate_pair_t* pair;
	assert(ice_candidate_pairs_count(component) > 0);
	pair = ice_candidate_pairs_get((ice_candidate_pairs_t*)component, 0);
	return (int)(pair->local.component - *id);
}

static ice_candidate_pairs_t* ice_candidate_components_fetch(ice_candidate_components_t* components, uint16_t id)
{
	int pos;
	ice_candidate_pairs_t arr, *component;
	component = darray_find(components, &id, &pos, ice_candidate_component_compare);
	if (NULL == component)
	{
		darray_init(&arr, sizeof(struct ice_candidate_pair_t), 9);
		if (0 != darray_insert(components, pos, &arr, 1))
			return NULL;
		component = (ice_candidate_pairs_t*)darray_get(components, pos);
	}
	return component;
}

#endif /* !_ice_candidates_h_ */
