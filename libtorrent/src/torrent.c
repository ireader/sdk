#include "torrent.h"
#include "torrent-internal.h"
#include "torrent-sched.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static int torrent_init_pieces(torrent_t* tor, const struct metainfo_t* metainfo)
{
	size_t i;
	for (i = 0; i < tor->piece_count; i++)
	{
		tor->pieces[i] = piece_create(i, metainfo->piece_bytes, metainfo->pieces + i * 20);
		if (!tor->pieces[i])
			return -1;
	}
	return 0;
}

struct torrent_t* torrent_create(const struct metainfo_t* metainfo, const uint8_t id[20], uint16_t port, struct torrent_handler_t* handler, void* param)
{
	struct torrent_t* tor;
	assert(metainfo && handler && metainfo->piece_bytes >= N_PIECE_SLICE);
	tor = (struct torrent_t*)calloc(1, sizeof(*tor) + sizeof(struct piece_t*) * metainfo->piece_count);
	if (tor)
	{
		locker_create(&tor->locker);

		tor->meta = metainfo;
		tor->pieces = (struct piece_t**)(tor + 1);
		tor->piece_count = metainfo->piece_count;
		memcpy(tor->id, id, sizeof(tor->id));
		tor->port = port;

		tor->param = param;
		tor->sched = torrent_sched_create(tor, handler);
		tor->peers = heap_create(peer_compare_less, tor);

		if (0 != torrent_init_pieces(tor, metainfo))
		{
			torrent_destroy(tor);
			return NULL;
		}
	}
	return tor;
}

void torrent_destroy(struct torrent_t* tor)
{
	size_t i;
	struct peer_t* peer;
	for (i = 0; i < tor->meta->piece_count; i++)
	{
		if(tor->pieces[i])
			piece_destroy(tor->pieces[i]);
		tor->pieces[i] = NULL;
	}

	for (i = 0; i < heap_size(tor->peers); i++)
	{
		peer = (struct peer_t*)heap_get(tor->peers, i);
		peer_destroy(peer);
	}
	heap_destroy(tor->peers);

	torrent_sched_destroy(tor->sched);
	locker_destroy(&tor->locker);
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
	
	return torrent_sched_recv_piece(tor->sched, piece, tor->meta->piece_bytes, tor->meta->pieces + piece * 20);
}

int torrent_set_piece(torrent_t* tor, void* peer, uint32_t piece, uint32_t begin, uint32_t length, const void* data)
{
	return torrent_sched_send_piece(tor->sched, peer, piece, begin, length, data);
}

static struct peer_t* torrent_peer_find(torrent_t* tor, const struct sockaddr_storage* addr)
{
	int i;
	struct peer_t* peer;
	for (i = 0; i < heap_size(tor->peers); i++)
	{
		peer = (struct peer_t*)heap_get(tor->peers, i);
		if (0 == socket_addr_compare((const struct sockaddr*)&peer->base.addr, (const struct sockaddr*)addr))
			return peer;
	}
	return NULL;
}

int torrent_input_peers(torrent_t* tor, const struct sockaddr_storage* addrs, size_t count)
{
	int n;
	size_t i;
	struct peer_t* peer;

	n = 0;
	locker_lock(&tor->locker);
	for (i = 0; i < count; i++)
	{
		peer = torrent_peer_find(tor, addrs + i);
		if(peer)
			continue;

		peer = peer_create(aio, addrs + i, tor);
		heap_push(tor->peers, peer);
		n++;
	}
	locker_unlock(&tor->locker);
	return n;
}
