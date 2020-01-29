#include "bitmap.h"
#include "hweight.h"
#include <string.h>

#define BITS_PER_BYTE			(8)
#define BITS_TO_BYTES(nbits)	(((nbits) + BITS_PER_BYTE - 1) / BITS_PER_BYTE)
#define BITS_MASK_BYTE(nbits)	((uint8_t)~(((uint8_t)~0) >> nbits))

void bitmap_zero(uint8_t* bitmap, size_t nbits)
{
	size_t n = BITS_TO_BYTES(nbits);
	memset(bitmap, 0x00, n);
}

void bitmap_fill(uint8_t* bitmap, size_t nbits)
{
	size_t n = BITS_TO_BYTES(nbits);
	memset(bitmap, 0xFF, n);
}

void bitmap_copy(uint8_t *bitmap, const uint8_t *src, size_t nbits)
{
	size_t n = BITS_TO_BYTES(nbits);
	memmove(bitmap, src, n);
}

void bitmap_set(uint8_t *bitmap, size_t start, size_t len)
{
	size_t end = start + len;
	size_t from = start / BITS_PER_BYTE;
	uint8_t mask = ((uint8_t)~0) >> (start % BITS_PER_BYTE);
	while (from < end / BITS_PER_BYTE)
	{
		bitmap[from++] |= mask;
		mask = (uint8_t)~0;
	}

	if (end % BITS_PER_BYTE)
	{
		mask &= BITS_MASK_BYTE(end % BITS_PER_BYTE);
		bitmap[from] |= mask;
	}
}

void bitmap_clear(uint8_t *bitmap, size_t start, size_t len)
{
	size_t end = start + len;
	size_t from = start / BITS_PER_BYTE;
	uint8_t mask = BITS_MASK_BYTE(start % BITS_PER_BYTE);
	while (from < end / BITS_PER_BYTE)
	{
		bitmap[from++] &= mask;
		mask = 0UL;
	}

	if (end % BITS_PER_BYTE)
	{
		mask |= ((uint8_t)~0) >> (end % BITS_PER_BYTE);
		bitmap[from] &= mask;
	}
}

void bitmap_or(uint8_t* result, const uint8_t* src1, const uint8_t* src2, size_t nbits)
{
	size_t i;
	for (i = 0; i < BITS_TO_BYTES(nbits); i++)
		result[i] = src1[i] | src2[i];
}

void bitmap_and(uint8_t* result, const uint8_t* src1, const uint8_t* src2, size_t nbits)
{
	size_t i;
	for (i = 0; i < BITS_TO_BYTES(nbits); i++)
		result[i] = src1[i] & src2[i];
}

void bitmap_xor(uint8_t* result, const uint8_t* src1, const uint8_t* src2, size_t nbits)
{
	size_t i;
	for (i = 0; i < BITS_TO_BYTES(nbits); i++)
		result[i] = src1[i] ^ src2[i];
}

size_t bitmap_weight(const uint8_t* bitmap, size_t nbits)
{
	size_t w;
	size_t i;
	const unsigned long *p;
	const uint8_t* p8;
	
	p = (unsigned long *)bitmap;
	for (i = w = 0; i < nbits / (BITS_PER_BYTE * sizeof(unsigned long)); i++)
		w += hweight_long(*(p + i));

	p8 = (const uint8_t*)(p + i);
	i = (nbits % (BITS_PER_BYTE * sizeof(unsigned long))) / BITS_PER_BYTE;
	while(i-- > 0)
		w += hweight8(*p8++);
	
	nbits = nbits % BITS_PER_BYTE;
	if(nbits)
		w += hweight8(*p8 & BITS_MASK_BYTE(nbits));
	return w;
}

#if 0
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
#endif

// Count Leading Zeros
static inline unsigned int clz8(uint8_t v)
{
	unsigned int num = BITS_PER_BYTE;
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
}

/// Find First Zero
static inline unsigned int ffz8(uint8_t v)
{
	return clz8((uint8_t)~v);
}

size_t bitmap_count_leading_zero(const uint8_t* bitmap, size_t nbits)
{
	size_t i, c = 0;
	size_t n = BITS_TO_BYTES(nbits);

	for (i = 0; i < n; i++)
	{
		c = clz8(bitmap[i]);
		if (c < BITS_PER_BYTE)
			break;
	}

	c += i * BITS_PER_BYTE;
	return c > nbits ? nbits : c;
}

size_t bitmap_count_next_zero(const uint8_t* bitmap, size_t nbits, size_t start)
{
	size_t c, i = start / BITS_PER_BYTE;
	uint8_t mask = ((uint8_t)~0) >> (start % BITS_PER_BYTE);

	c = clz8(bitmap[i] & mask);
	if (BITS_PER_BYTE == c && i + 1 < BITS_TO_BYTES(nbits))
		c += bitmap_count_leading_zero(bitmap + i + 1, nbits - (i + 1) * BITS_PER_BYTE);
	c += i * BITS_PER_BYTE;
	c = c > nbits ? nbits : c;
	c -= start;
	return c;
}

size_t bitmap_find_first_zero(const uint8_t* bitmap, size_t nbits)
{
	size_t i, c = 0;
	size_t n = BITS_TO_BYTES(nbits);

	for (i = 0; i < n; i++)
	{
		c = ffz8(bitmap[i]);
		if (c < BITS_PER_BYTE)
			break;
	}

	c += i * BITS_PER_BYTE;
	return c > nbits ? nbits : c;
}

size_t bitmap_find_next_zero(const uint8_t* bitmap, size_t nbits, size_t start)
{
	size_t c, i = start / BITS_PER_BYTE;
	uint8_t mask = BITS_MASK_BYTE(start % BITS_PER_BYTE);

	c = ffz8(bitmap[i] | mask);
	if (BITS_PER_BYTE == c && i + 1 < BITS_TO_BYTES(nbits))
		c += bitmap_find_first_zero(bitmap + i + 1, nbits - (i + 1) * BITS_PER_BYTE);
	c += i * BITS_PER_BYTE;
	c = c > nbits ? nbits : c;
	c -= start;
	return c;
}

int bitmap_test_bit(const uint8_t* bitmap, size_t bits)
{
	size_t n = bits / BITS_PER_BYTE;
	return bitmap[n] & (1 << (BITS_PER_BYTE - 1 - (bits % BITS_PER_BYTE)));
}
