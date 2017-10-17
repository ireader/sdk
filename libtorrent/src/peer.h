#ifndef _peer_h_
#define _peer_h_

#include "sys/sock.h"
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define N_PIECE_SLICE (1 << 14) // 16k

struct peer_base_t
{
	uint8_t flags[8];
	uint8_t id[20];

	unsigned int choke : 1; // don't send data to peer
	unsigned int interested : 1; // need something from peer
	unsigned int peer_choke : 1; // peer block me
	unsigned int peer_interested : 1; // peer request data

	uint8_t* bitfield;
	uint32_t bits;

	struct sockaddr_storage addr;
};

struct peer_handler_t
{
	int (*choke)(void* param, int choke);
	int (*interested)(void* param, int interested);
	int (*have)(void* param, uint32_t piece);
	int (*bitfield)(void* param, const uint8_t* bitfield, int count);
	int (*request)(void* param, uint32_t piece, uint32_t begin, uint32_t length);
	int (*piece)(void* param, uint32_t piece, uint32_t begin, uint32_t length, const void* data);
//	int (*cancel)(void* param, uint32_t piece, uint32_t begin, uint32_t length);

	int (*error)(void* param, int code);
};

typedef struct peer_t peer_t;

peer_t* peer_create(const struct sockaddr_storage* addr, struct peer_handler_t* handler, void* param);
void peer_destroy(peer_t* peer);

/// connect to peer, and handshake, bitfield
int peer_start(peer_t* peer, const uint8_t info_hash[20], const uint8_t id[20], uint16_t port, const char* version, const uint8_t* bitfield, uint32_t bits);
/// disconnect
int peer_stop(peer_t* peer);

int peer_choke(peer_t* peer, int choke);
int peer_interested(peer_t* peer, int interested);
int peer_have(peer_t* peer, uint32_t piece);
int peer_recv(peer_t* peer, uint32_t piece, uint32_t begin, uint32_t length);
int peer_cancel(peer_t* peer, uint32_t piece, uint32_t begin, uint32_t length);

int peer_send_slices(peer_t* peer, uint32_t piece, uint32_t begin, uint32_t length, const uint8_t* slices);
int peer_send_meta(peer_t* peer, const uint8_t* meta, uint32_t bytes);

int peer_get_host(peer_t* peer, char ip[SOCKET_ADDRLEN], uint16_t* port);
struct peer_base_t* peer_get_base(peer_t* peer);

#if defined(__cplusplus)
}
#endif
#endif /* !_peer_h_ */
