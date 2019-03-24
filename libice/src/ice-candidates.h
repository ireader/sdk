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

static inline void ice_candidates_count(ice_candidates_t* arr)
{
	darray_count(arr);
}

static inline const struct ice_candidate_t* ice_candidates_list(ice_candidates_t* arr)
{
	return (const struct ice_candidate_t*)arr->elements;
}

static inline int ice_candidates_find(ice_candidates_t* arr, const struct ice_candidate_t* c, int *before)
{
	int i;
	struct ice_candidate_t* v;
	before = before ? before : &i;
	for (*before = 0; *before < darray_count(arr); *before++)
	{
		v = (struct ice_candidate_t*)darray_get(arr, *before);
		if (v->priority == c->priority)
			return *before;
		else if (v->priority > c->priority)
			break;
	}

	return -1;
}

static inline int ice_candidates_insert(ice_candidates_t* arr, const struct ice_candidate_t* c)
{
	int before;
	if (-1 != ice_candidates_find(arr, c, &before))
		return -1; // EEXIST
	return darray_insert(arr, before, c, 1);
}

static inline int ice_candidates_remove(ice_candidates_t* arr, const struct ice_candidate_t* c)
{
	int n;
	n = ice_candidates_find(arr, c, NULL);
	if (-1 == n)
		return -1; // NOT FOUND
	return darray_erase(arr, n);
}

#endif /* !_ice_candidates_h_ */
