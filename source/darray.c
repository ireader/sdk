#include "darray.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <math.h>

#define SIZE(n)      (arr->size * (n))
#define ADDRESS(idx) ((char*)arr->elements + SIZE(idx))

static void* darray_realloc(struct darray_t* arr, size_t size)
{
	return realloc(arr->elements, size);
}

static void darray_ptrfree(struct darray_t* arr)
{
	free(arr->elements);
}

void darray_init(struct darray_t* arr, int size, int capacity)
{
    arr->count = 0;
    arr->size = size;
	arr->capacity = 0;
	arr->elements = NULL;
	if (!arr->alloc)
		arr->alloc = darray_realloc;
	if (!arr->free)
		arr->free = darray_ptrfree;

    size *= capacity;
    if (size > 0)
    {
        arr->elements = arr->alloc(arr, size);
        arr->capacity = arr->elements ? capacity : 0;
    }
    else
    {
        arr->elements = NULL;
        arr->capacity = 0;
    }
}

void darray_free(struct darray_t* arr)
{
    if (arr->elements)
    {
		arr->free(arr);
        arr->elements = NULL;
    }
    arr->capacity = 0;
    arr->count = 0;
}

void darray_clear(struct darray_t* arr)
{
	assert(arr->count >= 0);
	arr->count = 0;
}

int darray_erase(struct darray_t* arr, int index)
{
    if (index < 0 || index >= arr->count)
        return -ENOENT;

    if(index + 1 < arr->count)
        memmove(ADDRESS(index), ADDRESS(index + 1), SIZE(arr->count - index - 1));
    arr->count -= 1;
    return 0;
}

int darray_insert(struct darray_t* arr, int before, const void* item)
{
	int n;
    void* p;

    if (!item)
        return -EINVAL;

    if (arr->count + 1 > arr->capacity)
    {
		n = arr->count + 1 + (int)sqrt(arr->count);
        p = arr->alloc(arr, n * arr->size);
        if (!p)
            return -ENOMEM;
        arr->elements = p;
        arr->capacity = n;
    }

	if (before >= 0 && arr->count > before)
		memmove(ADDRESS(before + 1), ADDRESS(before), SIZE(arr->count - before));
	else
		before = arr->count;
    memcpy(ADDRESS(before), item, SIZE(1));
    arr->count += 1;
    return 0;
}

int darray_push_back(struct darray_t* arr, const void* item)
{
    return darray_insert(arr, -1, item);
}

int darray_pop_back(struct darray_t* arr)
{
	return darray_erase(arr, arr->count-1);
}

int darray_pop_front(struct darray_t* arr)
{
    return darray_erase(arr, 0);
}

int darray_count(const struct darray_t* arr)
{
    return arr->count;
}

void* darray_get(struct darray_t* arr, int index)
{
    if (index < 0 || index >= arr->count)
        return NULL;

    return (char*)arr->elements + (arr->size * index);
}

void* darray_find(const struct darray_t* arr, const void* item, int *pos, darray_compare compare)
{
	int i, r;
	void* v;
	pos = pos ? pos : &i;
	for (*pos = 0; *pos < darray_count(arr); *pos += 1)
	{
		v = darray_get((struct darray_t*)arr, *pos);
		r = compare ? compare(v, item) : (0 == memcmp(v, item, arr->size) ? 0 : -1);
		if (0 == r)
			return v;
		else if (r > 0)
			break;
	}

	return NULL;
}

int darray_insert2(struct darray_t* arr, const void* item, darray_compare compare)
{
	int pos;
	if (NULL != darray_find(arr, item, &pos, compare))
		return -1; // EEXIST
	return darray_insert(arr, pos, item);
}

int darray_erase2(struct darray_t* arr, const void* item, darray_compare compare)
{
	int pos;
	if (NULL == darray_find(arr, item, &pos, compare))
		return -1; // NOT FOUND
	return darray_erase(arr, pos);
}
