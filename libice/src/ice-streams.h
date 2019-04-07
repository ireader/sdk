#ifndef _ice_streams_h_
#define _ice_streams_h_

#include "darray.h"
#include "ice-checklist.h"

struct ice_stream_t
{
	int stream;
	int default;
	struct ice_checklist_t* checks;
};

typedef struct darray_t ice_streams_t;

static inline void ice_streams_init(ice_streams_t* arr)
{
	darray_init(arr, sizeof(struct ice_stream_t), 4);
}

static inline void ice_streams_free(ice_streams_t* arr)
{
	int i;
	struct ice_stream_t *s;
	for (i = 0; i < darray_count(arr); i++)
	{
		s = (struct ice_stream_t *)darray_get(arr, i);
		if (s && s->checks)
			ice_checklist_destroy(&s->checks);
	}
	darray_free(arr);
}

static inline int ice_streams_count(ice_streams_t* arr)
{
	return darray_count(arr);
}

static inline struct ice_stream_t* ice_streams_get(ice_streams_t* arr, int i)
{
	return (struct ice_stream_t*)darray_get(arr, i);
}

static inline const struct ice_stream_t* ice_streams_list(ice_streams_t* arr)
{
	return (const struct ice_stream_t*)arr->elements;
}

static int ice_streams_compare(const struct ice_stream_t* l, const struct ice_stream_t* r)
{
	return l->stream - r->stream;
}

static inline int ice_streams_insert(ice_streams_t* arr, const struct ice_stream_t* stream)
{
	return darray_insert2(arr, stream, ice_streams_compare);
}

static inline int ice_streams_remove(ice_streams_t* arr, const struct ice_stream_t* stream)
{
	return darray_erase2(arr, stream, ice_streams_compare);
}

#endif /* !_ice_streams_h_ */
