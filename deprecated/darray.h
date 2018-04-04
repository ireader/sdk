#ifndef _darray_h_
#define _darray_h_

#include <stdlib.h>

struct darray_t
{
    void* elements;
    int size; // per element size
    int count; 
    int capacity;
};

#define darray_int(arr, capacity) darray_init(arr, sizeof(int), capacity)
#define darray_char(arr, capacity) darray_init(arr, sizeof(char), capacity)
#define darray_float(arr, capacity) darray_init(arr, sizeof(float), capacity)
#define darray_double(arr, capacity) darray_init(arr, sizeof(double), capacity)

void darray_init(struct darray_t* arr, int size, int capacity);
void darray_free(struct darray_t* arr);

int darray_push(struct darray_t* arr, const void* items, int count);
int darray_pop(struct darray_t* arr);

int darray_count(struct darray_t* arr);

#endif /* !_darray_h_ */
