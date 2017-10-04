#ifndef _bsearch_h_
#define _bsearch_h_

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

/// Binary search in array
/// @param[in] key pointer to the keyword
/// @param[in] arr pointer to the first object of the array
/// @param[in] num Nbumber of elements in the array pointed to by arr
/// @param[in] size Ssze in bytes of each element in the array.
/// @reutrn NULL-not found, other-key address
void* bsearch(const void* key, const void* arr, size_t num, size_t size,
				int (*cmp)(const void* key, const void* elt));

#if defined(__cplusplus)
}
#endif
#endif /* !_bsearch_h_ */ 
