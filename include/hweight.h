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

#if defined(__cplusplus)
}
#endif
#endif /* !_hweight_h_ */
