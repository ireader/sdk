#include "torrent.h"
#include "torrent-internal.h"
#include "torrent-sched.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

struct torrent_t* torrent_create(const struct metainfo_t* metainfo, const uint8_t id[20], uint16_t port, struct torrent_handler_t* handler, void* param)
{
	struct torrent_t* tor;
	assert(metainfo && handler && metainfo->piece_bytes >= N_PIECE_SLICE);
	tor = (struct torrent_t*)calloc(1, sizeof(*tor) + (metainfo->piece_count + 7) / 8);
	if (tor)
	{
		tor->meta = metainfo;
		tor->bitfield = (uint8_t*)(tor + 1);
		memcpy(tor->id, id, sizeof(tor->id));
		tor->port = port;

		tor->param = param;
		tor->disp = torrent_sched_create(tor, handler);
	}
	return tor;
}

void torrent_destroy(struct torrent_t* tor)
{
	torrent_sched_destroy(tor->disp);
	free(tor);
}

int torrent_set_bitfield(struct torrent_t* tor, const uint8_t* bitfield, uint32_t bits)
{
	if (bits != tor->meta->piece_count)
		return -1;

	memcpy(tor->bitfield, bitfield, (bits + 7) / 8);
	return 0;
}

int torrent_get_bitfield(struct torrent_t* tor, uint8_t** bitfield, uint32_t* count)
{
	if (bitfield) *bitfield = tor->bitfield;
	if (count) *count = tor->meta->piece_count;
	return 0;
}

int torrent_get_piece(struct torrent_t* tor, uint32_t piece)
{
	if (piece >= tor->meta->piece_count)
		return -1;
	
	return torrent_sched_recv_piece(tor->disp, piece, tor->meta->piece_bytes, tor->meta->pieces + piece * 20);
}

int torrent_set_piece(torrent_t* tor, void* peer, uint32_t piece, uint32_t begin, uint32_t length, const void* data)
{
	return torrent_sched_send_piece(tor->disp, peer, piece, begin, length, data);
}

int torrent_input_peers(torrent_t* tor, const struct sockaddr_storage* addrs, size_t count)
{
	return torrent_sched_peers(tor->disp, addrs, count);
}
