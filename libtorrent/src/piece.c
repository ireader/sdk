#include "piece.h"
#include "peer.h"
#include "bitmap.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int hash_sha1(const uint8_t* data, unsigned int bytes, uint8_t sha1[20]);

#define PIECE_BYTES_TO_SLICES(bytes) ( (bytes + N_PIECE_SLICE - 1) / N_PIECE_SLICE )

struct piece_t* piece_create(uint32_t piece, uint32_t bytes, const uint8_t sha1[20])
{
	struct piece_t* p;

	assert(0 == bytes % N_PIECE_SLICE);
	bytes = PIECE_BYTES_TO_SLICES(bytes) * N_PIECE_SLICE;
	p = (struct piece_t*)malloc(sizeof(*p) + bytes + (PIECE_BYTES_TO_SLICES(bytes) + 7) / 8);
	if (!p) return NULL;

	memcpy(p->sha1, sha1, sizeof(p->sha1));
	p->piece = piece;
	p->bytes = bytes;
	p->data = (uint8_t *)(p + 1);
	p->bitfield = p->data + p->bytes;
	memset(p->bitfield, 0, (PIECE_BYTES_TO_SLICES(bytes) + 7) / 8);
	return p;
}

void piece_destroy(struct piece_t* piece)
{
	free(piece);
}

int piece_check(struct piece_t* piece)
{
	uint8_t sha1[20];
	hash_sha1(piece->data, piece->bytes, sha1);
	return 0 == memcmp(piece->sha1, sha1, 20) ? 1 : 0;
}

int piece_write(struct piece_t* piece, uint32_t begin, uint32_t length, const uint8_t* data)
{
	unsigned int bits;

	assert(0 == length % N_PIECE_SLICE);
	assert(begin + length <= piece->bytes);
	if (begin + length <= piece->bytes)
	{
		memcpy(piece->data + begin, data, length);
		bitmap_set(piece->bitfield, begin / N_PIECE_SLICE, length / N_PIECE_SLICE);
	}

	bits = PIECE_BYTES_TO_SLICES(piece->bytes);
	return bits == bitmap_weight(piece->bitfield, bits) ? 1 : 0;
}
