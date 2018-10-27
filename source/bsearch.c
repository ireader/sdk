#include "bsearch.h"

/*
void* bsearch(const void* key, const void* arr, size_t num, size_t size,
				int (*cmp)(const void* key, const void* elt))
{
	int result;
	void* ptr;
	size_t start, end, mid;

	start = 0;
	end = start + num;

	while (start < end)
	{
		mid = (start + end) / 2;
		ptr = (char*)arr + mid * size;

		result = cmp(key, ptr);
		if (result < 0)
			end = mid;
		else if (result > 0)
			start = mid + 1;
		else
			return ptr;
	}

	return NULL;
}
*/

/// @param[out] pos key position
/// @return 0-find, other-not found, pos is insert position
int bsearch2(const void* key, const void* arr, const void** pos, size_t num, size_t size,
	int(*cmp)(const void* key, const void* elt))
{
	int result;
	const void* ptr;
	size_t start, end, mid;

	result = -1;
	start = 0;
	end = start + num;
	ptr = arr;

	while (start < end)
	{
		mid = (start + end) / 2;
		ptr = (char*)arr + mid * size;

		result = cmp(key, ptr);
		if (result < 0)
			end = mid;
		else if (result > 0)
			start = mid + 1;
		else
			break;
	}

	if (pos) *pos = result > 0 ? ((char*)ptr + size) : ptr;
	return result;
}

#if defined(_DEBUG) || defined(DEBUG)
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
static int bsearch_int_compare(const void* key, const void* elt)
{
	return *(const int*)key - *(const int*)elt;
}

void bsearch_test(void)
{
	size_t i, j, num;
	int arr[10000], v, *p;

	srand((unsigned int)time(NULL));
	for (num = i = 0; i < sizeof(arr)/sizeof(arr[0]); i++)
	{
		v = rand();
		if (0 == bsearch2(&v, arr, (const void**)&p, num, sizeof(int), bsearch_int_compare))
		{
			assert(v == *p);
		}
		else
		{
			memmove(p + 1, p, (num - (p - arr)) * sizeof(int));
			*p = v;
			++num;

			for (j = 1; j < num; j++)
			{
				assert(arr[j - 1] < arr[j]);
			}
		}
	}

	printf("bsearch test ok\n");
}
#endif
