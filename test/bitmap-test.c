#include "bitmap.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define N 1001

void bitmap_test(void)
{
	unsigned int i, j, n;
	uint8_t bitmap[(N + 7)/8];
	uint8_t ffs[4] = { 0xEF, 0xCD, 0xA8, 0x19 };
	assert(18 == bitmap_weight(ffs, 32));
	assert(0 == bitmap_count_leading_zero(ffs, 32));
	assert(3 == bitmap_find_first_zero(ffs, 32));
	assert(0 == bitmap_count_next_zero(ffs, 32, 12));
	assert(2 == bitmap_find_next_zero(ffs, 32, 15));
	assert(5 == bitmap_count_next_zero(ffs, 32, 22));
	assert(4 == bitmap_find_next_zero(ffs, 32, 6));
	
	bitmap_zero(bitmap, N);
	assert(0 == bitmap_weight(bitmap, N));
	assert(N == bitmap_count_leading_zero(bitmap, N));
	for (i = 0; i < N; i++)
	{
		assert(!bitmap_test_bit(bitmap, i));
	}

	bitmap_fill(bitmap, N);
	assert(N == bitmap_weight(bitmap, N));
	assert(0 == bitmap_count_leading_zero(bitmap, N));
	for (i = 0; i < N; i++)
	{
		assert(bitmap_test_bit(bitmap, i));
	}

	srand(time(NULL));
	//srand(31415926);
	for (i = 0; i < sizeof(bitmap) / sizeof(bitmap[0]); i++)
		bitmap[i] = (uint8_t)(rand() * 31415926UL);

	n = bitmap_count_leading_zero(bitmap, N);
	assert(n == bitmap_count_next_zero(bitmap, N, 0));
	for (j = 0; j < N; j += n + 1)
	{
		n = bitmap_count_next_zero(bitmap, N, j);

		for (i = 0; i < n; i++)
		{
			assert(!bitmap_test_bit(bitmap, j + i));
		}

		assert(j + n == N || bitmap_test_bit(bitmap, j + n));
	}

	j = rand() % N;
	n = rand() % (N - j);
	bitmap_set(bitmap, j, n);
	for (i = j; i < j + n; i++)
	{
		assert(bitmap_test_bit(bitmap, i));
	}

	j = rand() % N;
	n = rand() % (N - j);
	bitmap_clear(bitmap, j, n);
	for (i = j; i < j + n; i++)
	{
		assert(!bitmap_test_bit(bitmap, i));
	}

	printf("bitmap test ok\n");
}
