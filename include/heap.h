#ifndef _heap_h_
#define _heap_h_

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct heap_t heap_t;

/// heap compare callback
/// @return 1 if ptr1 < ptr2, 0-other
typedef int (*heap_less)(void* param, const void* ptr1, const void* ptr2);

/// create heap
/// default min-heap, change heap_less behavor to create max-heap
heap_t* heap_create(heap_less compare, void* param);
void heap_destroy(heap_t* heap);

/// reserve heap capacity
/// if size <= capacity, do nothing
void heap_reserve(heap_t* heap, int size);

int heap_size(heap_t* heap);
int heap_empty(heap_t* heap);

int heap_push(heap_t* heap, void* ptr);
void heap_pop(heap_t* heap);
void* heap_top(heap_t* heap);

void* heap_get(heap_t* heap, int index);

#if defined(__cplusplus)
}
#endif
#endif /* !_heap_h_ */
