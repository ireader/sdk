#ifndef _ice_candidates_h_
#define _ice_candidates_h_

#include "darray.h"

typedef struct darray_t ice_candidates_t;

static inline void ice_candidates_init(ice_candidates_t* arr)
{
	darray_init(arr, sizeof(struct ice_candidate_t), 4);
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

static inline const struct ice_candidate_t* ice_candidates_list(ice_candidates_t* arr)
{
	return (const struct ice_candidate_t*)arr->elements;
}

static int ice_candidates_compare(const struct ice_candidate_t* l, const struct ice_candidate_t* r)
{
	return (int)(l->priority - r->priority);
}

static inline int ice_candidates_insert(ice_candidates_t* arr, const struct ice_candidate_t* c)
{
	return darray_insert2(arr, c, ice_candidates_compare);
}

static inline int ice_candidates_remove(ice_candidates_t* arr, const struct ice_candidate_t* c)
{
	return darray_erase2(arr, c, ice_candidates_compare);
}

//////////////////////////////////////////////////////////////////////////
typedef struct darray_t ice_candidate_pairs_t;

static inline void ice_candidate_pairs_init(ice_candidate_pairs_t* arr)
{
	darray_init(arr, sizeof(struct ice_candidate_pair_t), 9);
}

static inline void ice_candidate_pairs_free(ice_candidate_pairs_t* arr)
{
	darray_free(arr);
}

static inline int ice_candidate_pairs_count(ice_candidate_pairs_t* arr)
{
	return darray_count(arr);
}

static inline struct ice_candidate_pair_t* ice_candidate_pairs_get(ice_candidate_pairs_t* arr, int i)
{
	return (struct ice_candidate_pair_t*)darray_get(arr, i);
}

static inline const struct ice_candidate_pair_t* ice_candidate_pairs_list(ice_candidate_pairs_t* arr)
{
	return (const struct ice_candidate_pair_t*)arr->elements;
}

static int ice_candidate_pairs_compare(const struct ice_candidate_pair_t* l, const struct ice_candidate_pair_t* r)
{
	if (l->priority == r->priority)
	{
		if (0 != memcmp(&l->local, &r->local, sizeof(struct ice_candidate_t)))
			return memcmp(&l->local, &r->local, sizeof(struct ice_candidate_t));
		if (0 != memcmp(&l->remote, &r->remote, sizeof(struct ice_candidate_t)))
			return memcmp(&l->remote, &r->remote, sizeof(struct ice_candidate_t));
		return 0;
	}
	return l->priority > r->priority ? 1 : -1;
}

static inline int ice_candidate_pairs_insert(ice_candidate_pairs_t* arr, const struct ice_candidate_pair_t* pair)
{
	return darray_insert2(arr, pair, ice_candidate_pairs_compare);
}

static inline int ice_candidate_pairs_remove(ice_candidate_pairs_t* arr, const struct ice_candidate_pair_t* pair)
{
	return darray_erase2(arr, pair, ice_candidate_pairs_compare);
}

#endif /* !_ice_candidates_h_ */
