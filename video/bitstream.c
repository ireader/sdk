#include "bitstream.h"
#include <stdlib.h>
#include <assert.h>

#define BIT_NUM (sizeof(char)*8)

bitstream_t* bitstream_create(const unsigned char* stream, int bytes)
{
	int i, j;
	bitstream_t* o;

	o = (bitstream_t*)malloc(sizeof(bitstream_t) + bytes);
	if(!o)
		return o;

	//o->stream = stream;
	o->stream = (unsigned char*)(o + 1);
	for(i=j=0; i<bytes; i++)
	{
		if(stream[i] == 0x03 && i>1 && stream[i-1]==0 && stream[i-2]==0)
			continue;

		o->stream[j++] = stream[i];
	}
	o->bytes = j;
	o->offsetBits = 0;
	o->offsetBytes = 0;
	return o;
}

void bitstream_destroy(bitstream_t* stream)
{
	if(stream)
		free(stream);
}

int bitstream_get_offset(bitstream_t* stream, int* bytes, int* bits)
{
	*bytes = stream->offsetBytes;
	*bits = stream->offsetBits;
	return 0;
}

int bitstream_set_offset(bitstream_t* stream, int bytes, int bits)
{
	if(bytes > stream->bytes || bits > sizeof(unsigned char))
		return -1;

	stream->offsetBits = bits;
	stream->offsetBytes = bytes;
	return 0;
}

int bitstream_next_bit(bitstream_t* stream)
{
	assert(stream && stream->stream && stream->bytes>0);
	if(stream->offsetBytes >= stream->bytes)
		return 0; // throw exception

	return (stream->stream[stream->offsetBytes] >> (BIT_NUM-1-stream->offsetBits)) & 0x01;
}

int bitstream_next_bits(bitstream_t* stream, int bits)
{
	int i, bit, value;
	int offsetBytes, offsetBits;

	offsetBits = stream->offsetBits;
	offsetBytes = stream->offsetBytes;
	assert(stream && stream->stream && stream->bytes>0);

	for(value = i = 0; i < bits; i++)
	{
		if(offsetBytes >= stream->bytes)
			return value; // throw exception

		bit = (stream->stream[offsetBytes] >> (BIT_NUM-1-offsetBits)) & 0x01;

		assert(0 == bit || 1 == bit);
		value = (value << 1) | bit;
		offsetBytes += (offsetBits + 1) / BIT_NUM;
		offsetBits = (offsetBits + 1) % BIT_NUM;
	}
	return value;
}

int bitstream_read_bit(bitstream_t* stream)
{
	int bit;
	assert(stream && stream->stream && stream->bytes>0);
	if(stream->offsetBytes >= stream->bytes)
		return 0; // throw exception

	bit = (stream->stream[stream->offsetBytes] >> (BIT_NUM-1-stream->offsetBits)) & 0x01;

	// update offset
	stream->offsetBytes += (stream->offsetBits + 1) / BIT_NUM;
	stream->offsetBits = (stream->offsetBits + 1) % BIT_NUM;

	assert(0 == bit || 1 == bit);
	return bit;
}

int bitstream_read_bits(bitstream_t* stream, int bits)
{
	int i, bit, value;

	assert(stream && bits > 0 && bits <= 32);
	value = 0;
	for(i=0; i<bits; i++)
	{
		bit = bitstream_read_bit(stream);
		assert(0 == bit || 1 == bit);
		value = (value << 1) | bit;
	}
	return value;
}

int bitstream_read_ue(bitstream_t* stream)
{
	int bit;
	int leadingZeroBits = -1;
	for(bit = 0; !bit; ++leadingZeroBits)
	{
		bit = bitstream_read_bit(stream);
		assert(0 == bit || 1 == bit);
	}

	bit = 0;
	if(leadingZeroBits > 0)
	{
		assert(leadingZeroBits < 32);
		bit = bitstream_read_bits(stream, leadingZeroBits);
	}
	return (1 << leadingZeroBits) -1 + bit;
}

int bitstream_read_se(bitstream_t* stream)
{
	int v = bitstream_read_ue(stream);
	return (0==v%2 ? -1 : 1) * ((v + 1) / 2);
}

int bitstream_read_me(bitstream_t* stream, int chroma_format_idc, int coded_block_pattern)
{
	static int intra[48] = {0, 16, 1, 2, 4, 8, 32, 3, 5, 10, 12, 15, 47, 7, 11, 13, 14, 6, 9, 31, 35, 37, 42, 44, 33, 34, 36, 40, 39, 43, 45, 46, 17, 18, 20, 24, 19, 21, 26, 28, 23, 27, 29, 30, 22, 25, 38, 41};
	static int intra_4x4_8x8[48] = {47, 31, 15, 0, 23, 27, 29, 30, 7, 11, 13, 14, 39, 43, 45, 46, 16, 3, 5, 10, 12, 19, 21, 26, 28, 35, 37, 42, 44, 1, 2, 4, 8, 17, 18, 20, 24, 6, 9, 22, 25, 32, 33, 34, 36, 40, 38, 41};

	static int chroma_intra[16] = {15, 0, 7, 11, 13, 14, 3, 5, 10, 12, 1, 2, 4, 8, 6, 9};
	static int chroma_intra_4x4_8x8[16] = {0, 1, 2, 4, 8, 3, 5, 10, 12, 15, 7, 11, 13, 14, 6, 9};

	int v = bitstream_read_ue(stream);
	if(chroma_format_idc)
	{
		assert(v >= 0 && v < 48);
		return coded_block_pattern ? intra[v] : intra_4x4_8x8[v];
	}
	else
	{
		assert(v >= 0 && v < 16);
		return coded_block_pattern ? chroma_intra[v] : chroma_intra_4x4_8x8[v];
	}
}

int bitstream_read_te(bitstream_t* stream)
{
	int v = bitstream_read_ue(stream);
	if(v != 1)
		return v;
	else
		return bitstream_read_bit(stream) ? 0 : 1;
}
