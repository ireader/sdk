#ifndef _darray_h_
#define _darray_h_

#include <stdlib.h>

struct darray_t
{
    void* elements;
    int size; // per element size
    int count; 
    int capacity;

	void* (*alloc)(struct darray_t* arr, size_t size);
	void (*free)(struct darray_t* arr);
};

#define darray_int(arr, capacity) darray_init(arr, sizeof(int), capacity)
#define darray_char(arr, capacity) darray_init(arr, sizeof(char), capacity)
#define darray_float(arr, capacity) darray_init(arr, sizeof(float), capacity)
#define darray_double(arr, capacity) darray_init(arr, sizeof(double), capacity)

void darray_init(struct darray_t* arr, int size, int capacity);
void darray_free(struct darray_t* arr);
void darray_clear(struct darray_t* arr);

/// @param[in] index delete item at index
int darray_erase(struct darray_t* arr, int index);
/// @param[in] before insert items before index, -1-last
int darray_insert(struct darray_t* arr, int before, const void* item);

int darray_push_back(struct darray_t* arr, const void* item);
int darray_pop_back(struct darray_t* arr);
int darray_pop_front(struct darray_t* arr);

int darray_count(const struct darray_t* arr);
void* darray_get(struct darray_t* arr, int index);

typedef int (*darray_compare)(const void* l, const void* r);

void* darray_find(const struct darray_t* arr, const void* item, int *pos, darray_compare compare);
int darray_insert2(struct darray_t* arr, const void* item, darray_compare compare);
int darray_erase2(struct darray_t* arr, const void* item, darray_compare compare);

#endif /* !_darray_h_ */
