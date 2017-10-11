#include "bsearch.h"

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
