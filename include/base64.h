#ifndef _base64_h_
#define _base64_h_

#include <stdlib.h>

#ifdef  __cplusplus
extern "C" {
#endif

/// base64 encode
/// @param[out] target output string buffer, target size = (source + 2)/3 * 4 + source / 57 (76/4*3)
/// @param[in] source input binary buffer
/// @param[in] bytes input buffer size in byte
/// @return target bytes
size_t base64_encode(char* target, const void *source, size_t bytes);
/// same as base64_encode except "+/" -> "-_"
size_t base64_encode_url(char* target, const void *source, size_t bytes);

/// base64 decode
/// @param[in] source input string buffer
/// @param[in] bytes input buffer size in byte
/// @param[out] target output binary buffer, target size = (source + 3)/4 * 3;
/// @return target bytes
size_t base64_decode(void* target, const char *source, size_t bytes);

/// Base16(HEX) Encoding
/// @return target bytes
size_t base16_encode(char* target, const void *source, size_t bytes);
size_t base16_decode(void* target, const char *source, size_t bytes);

size_t base32_encode(char* target, const void *source, size_t bytes);
size_t base32_decode(void* target, const char *source, size_t bytes);

#ifdef  __cplusplus
} // extern "C"
#endif

#endif /* !_base64_h_ */
