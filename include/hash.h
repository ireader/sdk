/* Fast hashing routine for ints,  longs and pointers.
	(C) 2002 Nadia Yvette Chambers, IBM */

/*
	linux/include/hash.h
	http://www.citi.umich.edu/techreports/reports/citi-tr-00-1.pdf
*/

#ifndef _hash_h_
#define _hash_h_

#include <stdint.h>

#if defined(_WIN64) || defined(_LP64)
#define hash_long(value, bits) hash_int64(value, bits)
#else
#define hash_long(value, bits) hash_int32(value, bits)
#endif

/* 2^31 + 2^29 - 2^25 + 2^22 - 2^19 - 2^16 + 1 */
#define GOLDEN_RATIO_PRIME_32 0x9e370001UL
/*  2^63 + 2^61 - 2^57 + 2^54 - 2^51 - 2^18 + 1 */
#define GOLDEN_RATIO_PRIME_64 0x9e37fffffffc0001UL

static inline uint64_t hash_int64(uint64_t value, unsigned int bits)
{
#if 1
	value = value * GOLDEN_RATIO_PRIME_64;
#else
	uint64_t n = value;
	n <<= 18;
	value -= n;
	n <<= 33;
	value -= n;
	n <<= 3;
	value += n;
	n <<= 3;
	value -= n;
	n <<= 4;
	value += n;
	n <<= 2;
	value += n;
#endif

	/* High bits are more random, so use them. */
	return value >> (64 - bits);
}

static inline uint32_t hash_int32(uint32_t value, unsigned int bits)
{
	value = value * GOLDEN_RATIO_PRIME_32;

	/* High bits are more random, so use them. */
	return value >> (32 - bits);
}

static inline unsigned long hash_ptr(const void *ptr, unsigned int bits)
{
	return hash_long((intptr_t)ptr, bits);
}

#endif /* !_hash_h_ */
