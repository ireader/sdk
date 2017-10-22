#ifndef _torrent_h_
#define _torrent_h_

#include "sys/sock.h"
#include "metainfo.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct torrent_t torrent_t;

enum { NOTIFY_NEED_MORE_PEER };

struct torrent_handler_t
{
	void (*notify)(void* param, int notify);
	void (*piece)(void* param, uint32_t piece, const void* data, uint32_t bytes);
	void (*request)(void* param, void* peer, uint32_t piece, uint32_t begin, uint32_t length);
};

torrent_t* torrent_create(const struct metainfo_t* metainfo, const uint8_t id[20], uint16_t port, struct torrent_handler_t* handler, void* param);
void torrent_destroy(torrent_t* tor);

int torrent_set_bitfield(torrent_t* tor, const uint8_t* bitfield, uint32_t bits);
int torrent_get_bitfield(torrent_t* tor, uint8_t** bitfield, uint32_t* bits);

int torrent_input_peers(torrent_t* tor, const struct sockaddr_storage* addrs, size_t count);

int torrent_get_piece(torrent_t* tor, uint32_t piece);
int torrent_set_piece(torrent_t* tor, void* peer, uint32_t piece, uint32_t begin, uint32_t length, const void* data);

int torrent_peer_id(const char* usr, uint8_t id[20]);

#if defined(__cplusplus)
}
#endif
#endif /* !_torrent_h_ */
