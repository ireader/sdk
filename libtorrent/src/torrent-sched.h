#ifndef _torrent_sched_h_
#define _torrent_sched_h_

#include "peer.h"
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct torrent_t;
struct torrent_handler_t;
typedef struct torrent_sched_t torrent_sched_t;

torrent_sched_t* torrent_sched_create(struct torrent_t* tor, struct torrent_handler_t* handler);
void torrent_sched_destroy(torrent_sched_t* disp);

int torrent_sched_peers(torrent_sched_t* disp, const struct sockaddr_storage* addrs, size_t count);

int torrent_sched_recv_piece(torrent_sched_t* disp, uint32_t piece, uint32_t bytes, const uint8_t hash[20]);

int torrent_sched_send_piece(torrent_sched_t* disp, void* peer, uint32_t piece, uint32_t begin, uint32_t length, const void* data);

#if defined(__cplusplus)
}
#endif
#endif /* !_torrent_sched_h_ */
