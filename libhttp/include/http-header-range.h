#ifndef _http_header_range_h_
#define _http_header_range_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct http_header_range_t
{
	// The first-byte-pos value in a byte-range-spec gives the byte-offset of the first byte in a range
	// Byte offsets start at zero.
	int64_t start;

	// The last-byte-pos value gives the byte-offset of the last byte in the range; 
	// that is, the byte positions specified are inclusive
	// if start == -1, used to specify the suffix of the entity-body.
	// If the entity is shorter than the specified suffix-length, the entire entity-body is used.
	int64_t end;
};

/*
Examples of byte-ranges-specifier values (assuming an entity-body of length 10000):
1. The first 500 bytes (byte offsets 0-499, inclusive): bytes=0-499
2. The second 500 bytes (byte offsets 500-999, inclusive): bytes=500-999
3. The final 500 bytes (byte offsets 9500-9999, inclusive): bytes=-500 Or bytes=9500-
4. The first and last bytes only (bytes 0 and 9999): bytes=0-0,-1
5. Several legal but not canonical specifications of the second 500 bytes (byte offsets 500-999, inclusive):
	bytes=500-600,601-999
	bytes=500-700,601-999
*/
int http_header_range(const char* field, struct http_header_range_t* range, int num);

#ifdef __cplusplus
}
#endif
#endif /* !_http_header_range_h_ */
