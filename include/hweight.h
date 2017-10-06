#ifndef _hweight_h_
#define _hweight_h_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/// @return the hamming weight of a N-bit word
int hweight8(uint8_t w);
int hweight16(uint16_t w);
int hweight32(uint32_t w);
int hweight64(uint64_t w);

static inline int hweight_long(unsigned long w)
{
	return sizeof(w) == 8 ? hweight64(w) : hweight32(w);
}

#if defined(__cplusplus)
}
#endif
#endif /* !_hweight_h_ */
