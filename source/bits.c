#include "bits.h"
#include <assert.h>

#define BIT_NUM_8	8
//#define BIT_NUM (sizeof(char)*8)

void bits_init(struct bits_t* bits, const void* data, size_t bytes)
{
	bits->data = (const uint8_t*)data;
	bits->bytes = bytes;
	bits->offsetBits = 0;
	bits->offsetBytes = 0;
}

static void bitstream_move(struct bits_t* bits, int n)
{
	bits->offsetBytes += (bits->offsetBits + n) / BIT_NUM_8;
	bits->offsetBits = (bits->offsetBits + n) % BIT_NUM_8;
}

int bits_next(struct bits_t* bits)
{
	uint8_t bit;
	assert(bits && bits->data && bits->bytes > 0);
	if (bits->offsetBytes >= bits->bytes)
		return -1; // throw exception

	bit = bits->data[bits->offsetBytes] & (0x80U >> bits->offsetBits);
	return bit ? 1 : 0;
}

int bits_next2(struct bits_t* bits, int n)
{
	int i, v;

	assert(n > 0 && n <= 32);
	assert(bits && bits->data && bits->bytes > 0);
	if (bits->offsetBytes >= bits->bytes)
		return -1; // throw exception

	v = bits->data[bits->offsetBytes] & (0xFFU >> bits->offsetBits); // remain valid value
	if (n <= BIT_NUM_8 - bits->offsetBits)
		return v >> (BIT_NUM_8 - bits->offsetBits - n); // shift right value

	n -= BIT_NUM_8 - bits->offsetBits;
	for (i = 1; n >= BIT_NUM_8 && bits->offsetBytes + i < bits->bytes; i++)
	{
		v <<= 8;
		v += bits->data[bits->offsetBytes + i];
		n -= 8;
	}

	if (n > 0 && bits->offsetBytes + i >= bits->bytes)
		return -1;

	if (n > 0)
	{
		v <<= n;
		v += bits->data[bits->offsetBytes + i] >> (BIT_NUM_8 - n);
	}
	return v;
}

int bits_read(struct bits_t* bits)
{
	int bit;
	bit = bits_next(bits);
	bitstream_move(bits, 1); // update offset
	return bit;
}

int bits_read2(struct bits_t* bits, int n)
{
	int bit;
	bit = bits_next2(bits, n);
	bitstream_move(bits, n); // update offset
	return bit;
}

int bits_read_ue(struct bits_t* bits)
{
	int bit;
	int leadingZeroBits = -1;
	for (bit = 0; !bit && -1 != bit; ++leadingZeroBits)
	{
		bit = bits_read(bits);
		assert(0 == bit || 1 == bit);
	}

	bit = 0;
	if (leadingZeroBits > 0)
	{
		assert(leadingZeroBits < 32);
		bit = bits_read2(bits, leadingZeroBits);
	}
	return (1 << leadingZeroBits) - 1 + bit;
}

int bits_read_se(struct bits_t* bits)
{
	int v = bits_read_ue(bits);
	return (0 == v % 2 ? -1 : 1) * ((v + 1) / 2);
}

int bits_read_te(struct bits_t* bits)
{
	int v = bits_read_ue(bits);
	if (v != 1)
		return v;
	else
		return bits_read(bits) ? 0 : 1;
}

#if defined(_DEBUG) || defined(DEBUG)
static void bits_test2(void)
{
	struct bits_t bits;
	// 1, 010, 011, 00100, 00101, 00110, 00111, 0001000, 0001001, 0001010, 0001011, 0001100, 0001101, 0001110, 0001111
	const uint8_t data[] = { 0xA6, 0x42, 0x98, 0xE2, 0x04, 0x8A, 0x16, 0x30, 0x68, 0xE1, 0xE0 };
	bits_init(&bits, data, sizeof(data));
	assert(0 == bits_read_ue(&bits));
	assert(1 == bits_read_ue(&bits));
	assert(2 == bits_read_ue(&bits));
	assert(3 == bits_read_ue(&bits));
	assert(4 == bits_read_ue(&bits));
	assert(5 == bits_read_ue(&bits));
	assert(6 == bits_read_ue(&bits));
	assert(7 == bits_read_ue(&bits));
	assert(8 == bits_read_ue(&bits));
	assert(9 == bits_read_ue(&bits));
	assert(10 == bits_read_ue(&bits));
	assert(11 == bits_read_ue(&bits));
	assert(12 == bits_read_ue(&bits));
	assert(13 == bits_read_ue(&bits));
	assert(14 == bits_read_ue(&bits));
}

void bits_test(void)
{
	struct bits_t bits;
	const uint8_t data[] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xCD, 0xAB };
	bits_init(&bits, data, sizeof(data));
	assert(0 == bits_read(&bits));
	assert(0 == bits_read(&bits));
	assert(0 == bits_read(&bits));
	assert(0 == bits_read(&bits));
	assert(0 == bits_read(&bits));
	assert(0 == bits_read(&bits));
	assert(0 == bits_read(&bits));
	assert(1 == bits_read(&bits));

	assert(0 == bits_read(&bits));
	assert(0 == bits_read(&bits));
	assert(1 == bits_read(&bits));
	assert(0 == bits_read(&bits));
	assert(0 == bits_read(&bits));
	assert(0 == bits_read(&bits));
	assert(1 == bits_read(&bits));
//	assert(1 == bits_read(&bits));

	assert(0x05 == bits_read2(&bits, 3));
	assert(0x15 == bits_read2(&bits, 8));
	assert(0x27 == bits_read2(&bits, 6));
	assert(0x08 == bits_read2(&bits, 4));
	assert(0x09ABCDEF == bits_read2(&bits, 28));

	assert(-1 == bits_read2(&bits, 17));
	bits_test2();
}
#endif
