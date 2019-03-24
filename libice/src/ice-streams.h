#ifndef _ice_streams_h_
#define _ice_streams_h_

#include "darray.h"
#include "ice-checklist.h"

struct ice_stream_t
{
	int stream;
	struct ice_checklist_t* l;
};

typedef struct darray_t ice_streams_t;

static inline void ice_streams_init(ice_streams_t* arr)
{
	darray_init(arr, sizeof(struct ice_stream_t), 4);
}

static inline void ice_streams_free(ice_streams_t* arr)
{
	darray_free(arr);
}

static inline void ice_streams_count(ice_streams_t* arr)
{
	darray_count(arr);
}

static inline const struct ice_stream_t* ice_streams_list(ice_streams_t* arr)
{
	return (const struct ice_stream_t*)arr->elements;
}

static inline int ice_streams_find(ice_streams_t* arr, const struct ice_stream_t* stream, int *before)
{
	int i;
	struct ice_stream_t* v;
	before = before ? before : &i;
	for (*before = 0; *before < darray_count(arr); *before++)
	{
		v = (struct ice_stream_t*)darray_get(arr, *before);
		if (v->stream == stream->stream)
			return *before;
		else if (v->stream > stream->stream)
			break;
	}

	return -1;
}

static inline int ice_streams_insert(ice_streams_t* arr, const struct ice_stream_t* stream)
{
	int before;
	if (-1 != ice_streams_find(arr, stream, &before))
		return -1; // EEXIST
	return darray_insert(arr, before, stream, 1);
}

static inline int ice_streams_remove(ice_streams_t* arr, const struct ice_stream_t* stream)
{
	int n;
	n = ice_streams_find(arr, stream, NULL);
	if (-1 == n)
		return -1; // NOT FOUND
	return darray_erase(arr, n);
}

#endif /* !_ice_streams_h_ */
