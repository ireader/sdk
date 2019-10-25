#ifndef _peer_h_
#define _peer_h_

#include "aio-socket.h"
#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define N_PIECE_SLICE (1 << 14) // 16k

struct peer_handler_t
{
	int (*handshake)(void* param, const uint8_t flags[8], const uint8_t info_hash[20], const uint8_t peer_id[20]);
	int (*choke)(void* param, int choke);
	int (*interested)(void* param, int interested);
	int (*have)(void* param, uint32_t piece);
	int (*bitfield)(void* param, const uint8_t* bitfield, uint32_t bits);
	int (*request)(void* param, uint32_t piece, uint32_t begin, uint32_t length);
	int (*piece)(void* param, uint32_t piece, uint32_t begin, uint32_t length, const void* data);
//	int (*cancel)(void* param, uint32_t piece, uint32_t begin, uint32_t length);
	int (*metadata)(void* param, const uint8_t info_hash[20]);
	int (*pex)(void* param, const uint8_t* flags, const struct sockaddr_storage* added, size_t n_added, const struct sockaddr_storage* dropped, size_t n_dropped);

	int (*error)(void* param, int code);
};

typedef struct peer_t peer_t;

peer_t* peer_create(aio_socket_t aio, const struct sockaddr_storage* addr, struct peer_handler_t* handler, void* param);
void peer_destroy(peer_t* peer);

int peer_handshake(peer_t* peer, const uint8_t info_hash[20], const uint8_t id[20]);
int peer_extended(peer_t* peer, uint16_t port, const char* version, uint32_t info_size);
int peer_bitfield(peer_t* peer, const uint8_t* bitfield, uint32_t bits);
int peer_choke(peer_t* peer, int choke);
int peer_interested(peer_t* peer, int interested);
int peer_have(peer_t* peer, uint32_t piece);
int peer_recv(peer_t* peer, uint32_t piece, uint32_t begin, uint32_t length);
int peer_cancel(peer_t* peer, uint32_t piece, uint32_t begin, uint32_t length);

int peer_send_slices(peer_t* peer, uint32_t piece, uint32_t begin, uint32_t length, const uint8_t* slices);
int peer_send_meta(peer_t* peer, const void* meta, uint32_t bytes);
int peer_empty(const peer_t* peer);

/// @return 1 if p1 < p2, 0-other
int peer_compare_less(void* param, const peer_t* p1, const peer_t* p2);

#if defined(__cplusplus)
}
#endif
#endif /* !_peer_h_ */
