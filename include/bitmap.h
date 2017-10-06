#ifndef _bitmap_h_
#define _bitmap_h_

#if defined(__cplusplus)
extern "C" {
#endif

void bitmap_zero(unsigned long* bitmap, unsigned int nbits);
void bitmap_fill(unsigned long* bitmap, unsigned int nbits);

void bitmap_set(unsigned long *bitmap, unsigned int start, unsigned int len);
void bitmap_clear(unsigned long *bitmap, unsigned int start, unsigned int len);

void bitmap_or(unsigned long* result, const unsigned long* src1, const unsigned long* src2, unsigned int nbits);
void bitmap_and(unsigned long* result, const unsigned long* src1, const unsigned long* src2, unsigned int nbits);
void bitmap_xor(unsigned long* result, const unsigned long* src1, const unsigned long* src2, unsigned int nbits);

unsigned int bitmap_count_leading_zero(const unsigned long* bitmap, unsigned int nbits);
unsigned int bitmap_count_next_zero(const unsigned long* bitmap, unsigned int nbits, unsigned int start);
unsigned int bitmap_weight(const unsigned long* bitmap, unsigned int nbits);

/// @return 0-not set, other-set to 1
int bitmap_test_bit(const unsigned long* bitmap, unsigned int bits);

#if defined(__cplusplus)
}
#endif
#endif /* !_bitmap_h_ */
