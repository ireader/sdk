#ifndef _peer_parser_h_
#define _peer_parser_h_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct peer_parser_t
{
	int state;
	uint32_t len;
	uint8_t type;

	uint8_t* buffer;
	size_t capacity;
	size_t bytes;
};

int peer_input(struct peer_parser_t* parser, const uint8_t* data, size_t bytes, int(*handler)(void* param, struct peer_parser_t*), void* param);

#if defined(__cplusplus)
}
#endif
#endif /* !_peer_parser_h_ */
