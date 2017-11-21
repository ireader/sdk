#ifndef _bsearch_h_
#define _bsearch_h_

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

/// Binary search in array
/// @param[in] key pointer to the keyword
/// @param[in] arr pointer to the first object of the array
/// @param[out] pos key position
/// @param[in] num Number of elements in the array pointed to by arr
/// @param[in] size Size in bytes of each element in the array.
/// @return 0-find, other-not found, pos is insert position
int bsearch2(const void* key, const void* arr, const void** pos, size_t num, size_t size,
	int(*cmp)(const void* key, const void* elt));

#if defined(__cplusplus)
}
#endif
#endif /* !_bsearch_h_ */ 
