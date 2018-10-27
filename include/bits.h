#ifndef _bits_h_
#define _bits_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bits_t
{
	const uint8_t* data;
	size_t bytes;
	size_t offsetBytes; // offset byte
	int offsetBits; // offset bit
};

void bits_init(struct bits_t* bits, const void* data, size_t bytes);

/// get 1-bit from bit stream(don't offset position)
/// @param[in] bits bit stream
/// @return -1-error, 1-value, 0-value
int bits_next(struct bits_t* bits);

/// read n-bit(n <= 32) from bit stream(don't offset position)
/// @param[in] bits bit stream
/// @return -1-error, 1-value, 0-value
int bits_next2(struct bits_t* bits, int n);

/// read 1-bit from bit stream(offset position)
/// @param[in] bits bit stream
/// @return -1-error, 1-value, 0-value
int bits_read(struct bits_t* bits);

/// read n-bit(n <= 32) from bit stream(offset position)
/// @param[in] bits bit stream
/// @return -1-error, other-value
int bits_read2(struct bits_t* bits, int n);

/// Exp-Golomb codes
int bits_read_ue(struct bits_t* bits);
int bits_read_se(struct bits_t* bits);
int bits_read_te(struct bits_t* bits);

#ifdef __cplusplus
}
#endif
#endif /* _bits_h_ */
