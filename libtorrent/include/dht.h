#ifndef _dht_h_
#define _dht_h_

#include <stdint.h>
#include "sys/sock.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct dht_handler_t
{
	void (*ping)(void* param, int code);
	void (*find_node)(void* param, int code, const uint8_t id[20]);
	void (*get_peers)(void* param, int code, const uint8_t info_hash[20], const struct sockaddr_storage* peers, uint32_t count);
	void (*announce_peer)(void* param, const uint8_t info_hash[20], uint16_t port, const struct sockaddr_storage* peer);

	int (*query_peers)(void* param, const uint8_t info_hash[20], struct sockaddr_storage* peers, uint32_t count);
};

typedef struct dht_t dht_t;

dht_t* dht_create(const uint8_t id[20], uint16_t port, struct dht_handler_t* handler, void* param);
int dht_destroy(dht_t* dht);

// dht.libtorrent.org:25401 dec8ae697351ff4aec29cdbaabf2fbe3467cc267
// dht.transmissionbt.com:6881 3c00727348b3b8ed70baa1e1411b3869d8481321
// router.bittorrent.com:6881 ebff36697351ff4aec29cdbaabf2fbe3467cc267
int dht_add_node(dht_t* dht, const uint8_t id[20], const struct sockaddr_storage* addr);
int dht_list_node(dht_t* dht, int (*onnode)(void* param, const uint8_t id[20], const struct sockaddr_storage* addr), void* param);

int dht_ping(dht_t* dht, const struct sockaddr_storage* addr);
int dht_find_node(dht_t* dht, const uint8_t id[20]);
int dht_get_peers(dht_t* dht, const uint8_t info_hash[20]);
int dht_announce(dht_t* dht, const struct sockaddr_storage* node, const uint8_t info_hash[20], uint16_t port, const uint8_t* token, uint32_t token_bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_dht_h_ */
