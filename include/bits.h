#ifndef _bits_h_
#define _bits_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bits_t
{
	uint8_t* data;
	size_t size;
	size_t bits; // offset bit
	int error;
};

#define bits_read_uint8(bits, n)		(uint8_t)bits_read_n(bits, n)
#define bits_read_uint16(bits, n)		(uint16_t)bits_read_n(bits, n)
#define bits_read_uint32(bits, n)		(uint32_t)bits_read_n(bits, n)
#define bits_read_uint64(bits, n)		(uint64_t)bits_read_n(bits, n)
#define bits_write_uint8(bits, v, n)	bits_write_n(bits, (uint64_t)v, n)
#define bits_write_uint16(bits, v, n)	bits_write_n(bits, (uint64_t)v, n)
#define bits_write_uint32(bits, v, n)	bits_write_n(bits, (uint64_t)v, n)
#define bits_write_uint64(bits, v, n)	bits_write_n(bits, (uint64_t)v, n)

void bits_init(struct bits_t* bits, const void* data, size_t size);

/// get 1-bit from bit stream(don't offset position)
/// @param[in] bits bit stream
/// @return -1-error, 1-value, 0-value
int bits_next(struct bits_t* bits);

/// read n-bit(n <= 64) from bit stream(don't offset position)
/// @param[in] bits bit stream
/// @return -1-error, 1-value, 0-value
uint64_t bits_next_n(struct bits_t* bits, int n);

/// read 1-bit from bit stream(offset position)
/// @param[in] bits bit stream
/// @return -1-error, 1-value, 0-value
int bits_read(struct bits_t* bits);

/// read n-bit(n <= 64) from bit stream(offset position)
/// @param[in] bits bit stream
/// @return -1-error, other-value
uint64_t bits_read_n(struct bits_t* bits, int n);

/// Exp-Golomb codes
int bits_read_ue(struct bits_t* bits);
int bits_read_se(struct bits_t* bits);
int bits_read_te(struct bits_t* bits);

/// write 1-bit to bit stream(offset position)
/// @param[in] bits bit stream
/// @param[in] v 0-0, other-1
/// @return 0-ok, other-error
int bits_write(struct bits_t* bits, int v);

/// write n-bit to bit stream(offset position)
/// @param[in] bits bit stream
/// @param[in] v value
/// @param[in] n value bit count
/// @return 0-ok, other-error
int bits_write_n(struct bits_t* bits, uint64_t v, int n);

#ifdef __cplusplus
}
#endif
#endif /* _bits_h_ */
