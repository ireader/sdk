#ifndef _piece_h_
#define _piece_h_

#include "sys/locker.h"
#include "list.h"
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct piece_t
{
	uint32_t bytes;
	uint32_t piece;
	uint8_t sha1[20]; // piece hash

	uint8_t *data;
	uint8_t *bitfield;

	locker_t locker;
	struct list_head idles; // idle peers
	struct list_head working; // working peers
};

struct piece_t* piece_create(uint32_t piece, uint32_t bytes, const uint8_t sha1[20]);
void piece_destroy(struct piece_t* piece);

/// write piece slice
/// @param[in] length slice length, should be N_PIECE_SLICE length
/// @return 1-bitfield write done, 0-don't finish yet
int piece_write(struct piece_t* piece, uint32_t begin, uint32_t length, const uint8_t* data);

/// hash check
/// @return 1-sha1 hash check ok, 0-failed
int piece_check(struct piece_t* piece);

#if defined(__cplusplus)
}
#endif
#endif /* !_piece_h_ */
