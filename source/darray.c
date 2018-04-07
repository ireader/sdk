#include "darray.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define SIZE(n)      (arr->size * (n))
#define ADDRESS(idx) ((char*)arr->elements + SIZE(idx))

void darray_init(struct darray_t* arr, int size, int capacity)
{
    arr->count = 0;
    arr->size = size;

    size *= capacity;
    if (size > 0)
    {
        arr->elements = malloc(size * capacity);
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
        free(arr->elements);
        arr->elements = NULL;
    }
    arr->capacity = 0;
    arr->count = 0;
}

int darray_erase(struct darray_t* arr, int index)
{
    if (index < 0 || index >= arr->count)
        return ENOENT;

    memmove(ADDRESS(index), ADDRESS(index + 1), SIZE(arr->count - 1));
    arr->count -= 1;
    return 0;
}

int darray_insert(struct darray_t* arr, int before, const void* items, int count)
{
    void* p;

    if (!items || count < 1 || before < 0 || before > arr->count)
        return EINVAL;

    if (arr->count + count > arr->capacity)
    {
        p = realloc(arr->elements, (arr->count + count + count / 2) * arr->size);
        if (!p)
            return ENOMEM;
        arr->elements = p;
        arr->capacity = arr->count + count + count / 2;
    }

    memmove(ADDRESS(before + count), ADDRESS(before), SIZE(arr->count - before));
    memcpy(ADDRESS(before), items, SIZE(count));
    arr->count += count;
    return 0;
}

int darray_push_back(struct darray_t* arr, const void* items, int count)
{
    return darray_insert(arr, arr->count, items, count);
}

int darray_pop_back(struct darray_t* arr)
{
	return darray_erase(arr, arr->count-1);
}

int darray_pop_front(struct darray_t* arr)
{
    return darray_erase(arr, 0);
}

int darray_count(struct darray_t* arr)
{
    return arr->count;
}

void* darray_get(struct darray_t* arr, int index)
{
    if (index < 0 || index >= arr->count)
        return NULL;

    return (char*)arr->elements + (arr->size * index);
}
