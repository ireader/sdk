#ifndef _dht_message_h_
#define _dht_message_h_

#include <stdint.h>
#include "node.h"

enum { DHT_PING, DHT_FIND_NODES, DHT_GET_PEERS, DHT_ANNOUNCE_PEER };

struct dht_message_handler_t
{
	int (*ping)(void* param, const uint8_t* transaction, uint32_t bytes, const uint8_t id[20]);
	int (*pong)(void* param, int code, const uint8_t id[20]);

	int (*find_node)(void* param, const uint8_t* transaction, uint32_t bytes, const uint8_t id[20], const uint8_t target[20]);
	int (*find_node_response)(void* param, int code, const uint8_t id[20], const struct node_t* nodes, uint32_t count);

	int (*get_peers)(void* param, const uint8_t* transaction, uint32_t bytes, const uint8_t id[20], const uint8_t info_hash[20]);
	int (*get_peers_response)(void* param, int code, const uint8_t id[20], const uint8_t* token, uint32_t bytes, const struct node_t* nodes, uint32_t count, const struct sockaddr_storage* peers, uint32_t peernum);

	int (*announce_peer)(void* param, const uint8_t* transaction, uint32_t bytes, const uint8_t id[20], const uint8_t info_hash[20], uint16_t port, const uint8_t* token, uint32_t token_bytes);
	int (*announce_peer_response)(void* param, int code, const uint8_t id[20]);
};

int dht_message_read(const uint8_t* buffer, uint32_t bytes, struct dht_message_handler_t* handler, void* param);

int dht_ping_write(uint8_t* buffer, uint32_t bytes, const uint8_t transaction[3], const uint8_t id[20]);
int dht_pong_write(uint8_t* buffer, uint32_t bytes, const uint8_t* transaction, uint32_t tidbytes, const uint8_t id[20]);

int dht_find_node_write(uint8_t* buffer, uint32_t bytes, const uint8_t transaction[3], const uint8_t id[20], const uint8_t target[20]);
int dht_find_node_reply_write(uint8_t* buffer, uint32_t bytes, const uint8_t* transaction, uint32_t tidbytes, const uint8_t id[20], const struct node_t* nodes, uint32_t count);

int dht_get_peers_write(uint8_t* buffer, uint32_t bytes, const uint8_t transaction[3], const uint8_t id[20], const uint8_t info_hash[20]);
int dht_get_peers_reply_write(uint8_t* buffer, uint32_t bytes, const uint8_t* transaction, uint32_t tidbytes, const uint8_t id[20], const uint8_t* token, uint32_t token_bytes, const struct node_t* nodes, uint32_t count, const struct sockaddr_storage* peers, uint32_t peernum);

int dht_announce_peer_write(uint8_t* buffer, uint32_t bytes, const uint8_t transaction[3], const uint8_t id[20], const uint8_t info_hash[20], uint16_t port, const uint8_t* token, uint32_t token_bytes);
int dht_announce_peer_reply_write(uint8_t* buffer, uint32_t bytes, const uint8_t* transaction, uint32_t tidbytes, const uint8_t id[20]);

#endif /* !_dht_message_h_ */
