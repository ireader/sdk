#include "bitmap.h"
#include "hweight.h"
#include <string.h>

#define BITS_PER_LONG			(sizeof(long) * 8)
#define BITS_TO_LONGS(nbits)	(((nbits) + BITS_PER_LONG - 1) / BITS_PER_LONG)
#define BITS_MASK_LONG(nbits)	(~((~0UL) >> nbits))

void bitmap_zero(unsigned long* bitmap, unsigned int nbits)
{
	unsigned int n = BITS_TO_LONGS(nbits);
	memset(bitmap, 0x00, n * sizeof(unsigned long));
}

void bitmap_fill(unsigned long* bitmap, unsigned int nbits)
{
	unsigned int n = BITS_TO_LONGS(nbits);
	memset(bitmap, 0xFF, n * sizeof(unsigned long));
}

void bitmap_set(unsigned long *bitmap, unsigned int start, unsigned int len)
{
	unsigned int end = start + len;
	unsigned int from = start / BITS_PER_LONG;
	unsigned long mask = (~0UL) >> (start % BITS_PER_LONG);
	while (from < end / BITS_PER_LONG)
	{
		bitmap[from++] |= mask;
		mask = ~0UL;
	}

	if (end % BITS_PER_LONG)
	{
		mask &= BITS_MASK_LONG(end % BITS_PER_LONG);
		bitmap[from] |= mask;
	}
}

void bitmap_clear(unsigned long *bitmap, unsigned int start, unsigned int len)
{
	unsigned int end = start + len;
	unsigned int from = start / BITS_PER_LONG;
	unsigned long mask = BITS_MASK_LONG(start % BITS_PER_LONG);
	while (from < end / BITS_PER_LONG)
	{
		bitmap[from++] &= mask;
		mask = 0UL;
	}

	if (end % BITS_PER_LONG)
	{
		mask |= (~0UL) >> (end % BITS_PER_LONG);
		bitmap[from] &= mask;
	}
}

void bitmap_or(unsigned long* result, const unsigned long* src1, const unsigned long* src2, unsigned int nbits)
{
	unsigned int i;
	for (i = 0; i < BITS_TO_LONGS(nbits); i++)
		result[i] = src1[i] | src2[i];
}

void bitmap_and(unsigned long* result, const unsigned long* src1, const unsigned long* src2, unsigned int nbits)
{
	unsigned int i;
	for (i = 0; i < BITS_TO_LONGS(nbits); i++)
		result[i] = src1[i] & src2[i];
}

void bitmap_xor(unsigned long* result, const unsigned long* src1, const unsigned long* src2, unsigned int nbits)
{
	unsigned int i;
	for (i = 0; i < BITS_TO_LONGS(nbits); i++)
		result[i] = src1[i] ^ src2[i];
}

unsigned int bitmap_weight(const unsigned long* bitmap, unsigned int nbits)
{
	int w;
	unsigned int i;
	
	for (i = w = 0; i < nbits / sizeof(unsigned long); i++)
		w += hweight_long(bitmap[i]);

	if (nbits % BITS_PER_LONG)
		w += hweight_long(bitmap[i] & BITS_MASK_LONG(nbits % BITS_PER_LONG));
	
	return w;
}

// https://en.wikipedia.org/wiki/Find_first_set
static inline unsigned int clz(unsigned long v)
{
#if defined(_MSC_VER)
#if defined(_WIN64)
	return __lzcnt64(v);
#else
	return __lzcnt(v);
#endif
#elif defined(__GNUC__)
	return __builtin_clzl(v);
#else
	unsigned int num = 64;
	if (8 == sizeof(v))
	{
		if (0xFFFFFFFF00000000ul & v)
		{
			num -= 32;
			v >>= 32;
		}
	}
	if (0xFFFF0000 & v)
	{
		num -= 16;
		v >>= 16;
	}
	if (0xFF00 & v)
	{
		num -= 8;
		v >>= 8;
	}
	if (0xF0 & v)
	{
		num -= 4;
		v >>= 4;
	}
	if (0xC & v)
	{
		num -= 2;
		v >>= 2;
	}
	if (0x2 & v)
	{
		num -= 1;
		v >>= 1;
	}
	if (0x1 & v)
		num -= 1;

	return num;
#endif
}

unsigned int bitmap_count_leading_zero(const unsigned long* bitmap, unsigned int nbits)
{
	unsigned int i, c = 0;
	unsigned int n = BITS_TO_LONGS(nbits);

	for (i = 0; i < n; i++)
	{
		c = clz(bitmap[i]);
		if (c < BITS_PER_LONG)
			break;
	}

	c += i * BITS_PER_LONG;
	return c > nbits ? nbits : c;
}

unsigned int bitmap_count_next_zero(const unsigned long* bitmap, unsigned int nbits, unsigned int start)
{
	unsigned int c, i = start / BITS_PER_LONG;
	unsigned int n = BITS_TO_LONGS(nbits);
	unsigned long mask = (~0UL) >> (start % BITS_PER_LONG);

	c = clz(bitmap[i] & mask);
	if (BITS_PER_LONG == c && i + 1 < BITS_TO_LONGS(nbits))
		c += bitmap_count_leading_zero(bitmap + i + 1, nbits - (i + 1) * BITS_PER_LONG);
	c += i * BITS_PER_LONG;
	c = c > nbits ? nbits : c;
	c -= start;
	return c;
}

int bitmap_test_bit(const unsigned long* bitmap, unsigned int bits)
{
	unsigned int n = bits / BITS_PER_LONG;
	return bitmap[n] & (1 << (BITS_PER_LONG - 1 - (bits % BITS_PER_LONG)));
}
