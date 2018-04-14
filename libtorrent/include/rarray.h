#ifndef _rarray_h_
#define _rarray_h_

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// Ring Array
struct rarray_t
{
	void* elements;
	int size; // per element size

	int count;
	int offset;
	int capacity;
};

static inline int rarray_init(struct rarray_t* arr, int size, int capacity)
{
	arr->count = 0;
	arr->size = size;
	arr->offset = 0;
	arr->elements = NULL;
	arr->capacity = capacity;
	
	size *= capacity;
	if (size > 0)
		arr->elements = malloc(size * capacity);

	return arr->elements ? 0 : ENOMEM;
}

static inline void rarray_free(struct rarray_t* arr)
{
	if (arr->elements)
	{
		free(arr->elements);
		arr->elements = NULL;
	}
	arr->count = 0;
}

static inline int rarray_count(struct rarray_t* arr)
{
	return arr->count;
}

static inline void* rarray_get(struct rarray_t* arr, int index)
{
	if (index < 0 || index >= arr->count)
		return NULL;

	return (char*)arr->elements + (arr->size * ((index + arr->offset) % arr->capacity));
}

// push back
static inline int rarray_push_back(struct rarray_t* arr, const void* element)
{
	void* p;

	if (!element || arr->count >= arr->capacity)
		return EINVAL;

	p = rarray_get(arr, arr->count + 1);
	memcpy(p, element, arr->size);
	arr->count += 1;
	return 0;
}

static inline int rarray_pop_front(struct rarray_t* arr)
{
	if (arr->count < 1)
		return ENOENT;

	arr->offset = (arr->offset + 1) % arr->capacity;
	arr->count -= 1;
	return 0;
}

static inline int rarray_pop_back(struct rarray_t* arr)
{
	if (arr->count < 1)
		return ENOENT;

	arr->count -= 1;
	return 0;
}

static inline void* rarray_front(struct rarray_t* arr)
{
	return rarray_get(arr, 0);
}

static inline int rarray_insert(struct rarray_t* arr, int before, const void* element)
{
	if (!element || arr->count >= arr->capacity || before < 0 || before > arr->count)
		return EINVAL;

	if (before + arr->count > arr->capacity)
	{
		memmove(rarray_get(arr, 1), rarray_get(arr, 0), arr->size * (before + arr->count - arr->capacity));
		memmove(rarray_get(arr, 0), rarray_get(arr, arr->count), arr->size * 1);
		memmove(rarray_get(arr, before + 1), rarray_get(arr, before), arr->capacity - before - 1);
	}
	else
	{
		memmove(rarray_get(arr, before + 1), rarray_get(arr, before), arr->count);
	}

	memcpy(rarray_get(arr, before), element, arr->size);
	arr->count += 1;
	return 0;
}

#endif /* !_rarray_h_ */
