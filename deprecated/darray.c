#include "darray.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

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

int darray_push(struct darray_t* arr, const void* items, int count)
{
    void* p;

    if (!items || count < 1)
        return EINVAL;

    if (arr->count + count > arr->capacity)
    {
        p = realloc(arr->elements, (arr->count + count + count / 2) * arr->size);
        if (!p)
            return ENOMEM;
        arr->elements = p;
        arr->capacity = arr->count + count + count / 2;
    }

    memcpy((char*)arr->elements + arr->count * arr->size, items, count * arr->size);
    arr->count += count;
    return 0;
}

int darray_pop(struct darray_t* arr)
{
    if (arr->count < 1)
        return ENOENT;

    memmove(arr->elements, (char*)arr->elements + arr->size, arr->count - 1);
    arr->count -= 1;
    return 0;
}

int darray_count(struct darray_t* arr)
{
    return arr->count;
}
