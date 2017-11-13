// http://www.bittorrent.org/beps/bep_0005.html (DHT Protocol)
// 1. Contact information for peers is encoded as a 6-byte string. 
//    Also known as "Compact IP-address/port info" the 4-byte IP address is in network byte order 
//    with the 2 byte port in network byte order concatenated onto the end.
// 2. Contact information for nodes is encoded as a 26-byte string. 
//    Also known as "Compact node info" the 20-byte Node ID in network byte order has 
//    the compact IP-address/port info concatenated to the end.

/*
ping
arguments:{"id" : "<querying nodes id>"}
response: {"id" : "<queried nodes id>"}

find_node
arguments:{"id" : "<querying nodes id>", "target" : "<id of target node>"}
response: {"id" : "<queried nodes id>", "nodes" : "<compact node info>"}

get_peers
arguments:{"id" : "<querying nodes id>", "info_hash" : "<20-byte infohash of target torrent>"}
response: {"id" : "<queried nodes id>", "token" :"<opaque write token>", "values" : ["<peer 1 info string>", "<peer 2 info string>"]}
	  or: {"id" : "<queried nodes id>", "token" :"<opaque write token>", "nodes" : "<compact node info>"}

announce_peer
arguments: {"id" : "<querying nodes id>",
			"implied_port": <0 or 1>,
			"info_hash" : "<20-byte infohash of target torrent>",
			"port" : <port number>,
			"token" : "<opaque token>"}
response: {"id" : "<queried nodes id>"}
*/

#include "dht-message.h"
#include "byte-order.h"
#include "bencode.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int bencode_write_item(uint8_t* buffer, uint32_t bytes, const char* key, const uint8_t* value, uint32_t size);
static int sockaddr_write(uint8_t* buffer, const struct sockaddr_storage* addr);
static const uint8_t* sockaddr_read(struct sockaddr_storage* addr, const uint8_t* buffer, int ipv6);
static int dht_message_read_find_node_reply(const struct bvalue_t *r, const struct bvalue_t *id, uint16_t transaction, struct dht_message_handler_t* handler, void* param);
static int dht_message_read_get_peers_reply(const struct bvalue_t *r, const struct bvalue_t *id, uint16_t transaction, struct dht_message_handler_t* handler, void* param);

int dht_ping_write(uint8_t* buffer, uint32_t bytes, const uint16_t transaction, const uint8_t id[20])
{
	int n;

	// ping Query = {"t":"aa", "y":"q", "q":"ping", "a":{"id":"abcdefghij0123456789"}}
	n = snprintf((char*)buffer, bytes, "d1:t3:%c--1:y1:q1:q4:ping1:ad2:id20:", DHT_PING);
	if (n < 0 || n + 2 + 20 >= (int)bytes)
		return -1;

	buffer[7] = (uint8_t)(transaction >> 8);
	buffer[8] = (uint8_t)transaction;
	memcpy(buffer + n, id, 20);
	memcpy(buffer + n + 20, "ee", 2);
	return n + 2 + 20;
}

int dht_pong_write(uint8_t* buffer, uint32_t bytes, const uint8_t* transaction, uint32_t tidbytes, const uint8_t id[20])
{
	int n;

	// Response = {"t":"aa", "y":"r", "r": {"id":"mnopqrstuvwxyz123456"}}
	n = snprintf((char*)buffer, bytes, "d1:t%u:", (unsigned int)tidbytes);
	if (n < 0 || n + tidbytes + 20 + 20 > bytes)
		return -1;
	memcpy(buffer + n, transaction, tidbytes);
	n += tidbytes;

	n += snprintf((char*)buffer + n, bytes - n, "1:y1:r1:rd2:id20:");
	memcpy(buffer + n, id, 20);
	memcpy(buffer + n + 20, "ee", 2);
	return n + 2 + 20;
}

// find_node Query = {"t":"aa", "y":"q", "q":"find_node", "a": {"id":"abcdefghij0123456789", "target":"mnopqrstuvwxyz123456"}}
// bencoded = d1:ad2:id20:abcdefghij01234567896:target20:mnopqrstuvwxyz123456e1:q9:find_node1:t2:aa1:y1:qe
// Response = {"t":"aa", "y":"r", "r": {"id":"0123456789abcdefghij", "nodes": "def456..."}}
// bencoded = d1:rd2:id20:0123456789abcdefghij5:nodes9:def456...e1:t2:aa1:y1:re
int dht_find_node_write(uint8_t* buffer, uint32_t bytes, const uint16_t transaction, const uint8_t id[20], const uint8_t target[20])
{
	int n;

	// find_node Query = {"t":"aa", "y":"q", "q":"find_node", "a": {"id":"abcdefghij0123456789", "target":"mnopqrstuvwxyz123456"}}
	n = snprintf((char*)buffer, bytes, "d1:t3:%c--1:y1:q1:q9:find_node1:ad2:id20:", DHT_FIND_NODE);
	if (n < 0 || n + 13 + 20 + 20 > (int)bytes)
		return -1;

	buffer[7] = (uint8_t)(transaction >> 8);
	buffer[8] = (uint8_t)transaction;
	memcpy(buffer + n, id, 20);
	memcpy(buffer + n + 20, "6:target20:", 11);
	memcpy(buffer + n + 31, target, 20);
	memcpy(buffer + n + 51, "ee", 2);
	return n + 53;
}

int dht_find_node_reply_write(uint8_t* buffer, uint32_t bytes, const uint8_t* transaction, uint32_t tidbytes, const uint8_t id[20], const struct node_t* nodes[], uint32_t count)
{
	int n, len = 0, len6 = 0;
	uint32_t i;

	for (i = 0; i < count; i++)
	{
		if (AF_INET == nodes[i]->addr.ss_family)
			len += 26;
		else if (AF_INET6 == nodes[i]->addr.ss_family)
			len6 += 38;
	}

	// Response = {"t":"aa", "y":"r", "r": {"id":"0123456789abcdefghij", "nodes": "def456..."}}
	n = snprintf((char*)buffer, bytes, "d1:t%u:", (unsigned int)tidbytes);
	if (n < 0 || n + tidbytes + 20 + len + len6 + 40 > bytes)
		return -1;
	memcpy(buffer + n, transaction, tidbytes);
	n += tidbytes;

	// id
	n += snprintf((char*)buffer + n, bytes - n, "1:y1:r1:rd2:id20:");
	memcpy(buffer + n, id, 20); 
	n += 20;

	// nodes
	if (len > 0)
	{
		n += snprintf((char*)buffer + n, bytes - n, "5:nodes%u:", (unsigned int)len);
		for (i = 0; i < count; i++)
		{
			if (AF_INET == nodes[i]->addr.ss_family)
			{
				memcpy(buffer + n, nodes[i]->id, 20);
				n += 20 + sockaddr_write(buffer + n + 20, &nodes[i]->addr);
			}
		}
	}

	// nodes6
	if (len6 > 0)
	{
		n += snprintf((char*)buffer + n, bytes - n, "6:nodes6%u:", (unsigned int)len6);
		for (i = 0; i < count; i++)
		{
			if (AF_INET6 == nodes[i]->addr.ss_family)
			{
				memcpy(buffer + n, nodes[i]->id, 20);
				n += 20 + sockaddr_write(buffer + n + 20, &nodes[i]->addr);
			}
		}
	}

	memcpy(buffer + n, "ee", 2);
	return n + 2;
}

int dht_get_peers_write(uint8_t* buffer, uint32_t bytes, const uint16_t transaction, const uint8_t id[20], const uint8_t info_hash[20])
{
	int n;

	// get_peers Query = {"t":"aa", "y":"q", "q":"get_peers", "a": {"id":"abcdefghij0123456789", "info_hash":"mnopqrstuvwxyz123456"}}
	n = snprintf((char*)buffer, bytes, "d1:t3:%c--1:y1:q1:q9:get_peers1:ad2:id20:", DHT_GET_PEERS);
	if (n < 0 || n + 2 + 20 + 34/*info_hash*/ >= (int)bytes)
		return -1;
	buffer[7] = (uint8_t)(transaction >> 8);
	buffer[8] = (uint8_t)transaction;
	memcpy(buffer + n, id, 20);
	n += 20;

	n += bencode_write_item(buffer + n, bytes - n, "info_hash", info_hash, 20);
	memcpy(buffer + n, "ee", 2);
	return n + 2;
}

int dht_get_peers_reply_write(uint8_t* buffer, uint32_t bytes, const uint8_t* transaction, uint32_t tidbytes, const uint8_t id[20], const uint8_t* token, uint32_t token_bytes, const struct node_t* nodes[], uint32_t count, const struct sockaddr_storage* peers, uint32_t peernum)
{
	int n;
	int nlen = 0, n6len = 0;
	int vlen = 0, v6len = 0;
	uint32_t i;

	for (i = 0; i < count; i++)
	{
		if (AF_INET == nodes[i]->addr.ss_family)
			nlen += 26;
		else if (AF_INET6 == nodes[i]->addr.ss_family)
			n6len += 38;
	}

	for (i = 0; i < peernum; i++)
	{
		if (AF_INET == peers[i].ss_family)
			vlen += 6;
		else if (AF_INET6 == peers[i].ss_family)
			v6len += 18;
	}

	// Response with closest nodes = {"t":"aa", "y":"r", "r": {"id":"abcdefghij0123456789", "token":"aoeusnth", "nodes": "def456...", "values": ["axje.u", "idhtnm"]}}
	n = snprintf((char*)buffer, bytes, "d1:t%u:", (unsigned int)tidbytes);
	if (n < 0 || n + tidbytes + 20 + nlen + n6len + vlen + v6len + 60 + token_bytes > bytes)
		return -1;
	memcpy(buffer + n, transaction, tidbytes);
	n += tidbytes;

	// id
	n += snprintf((char*)buffer + n, bytes - n, "1:y1:r1:rd2:id20:");
	memcpy(buffer + n, id, 20);
	n += 20;

	// token
	n += bencode_write_item(buffer + n, bytes - n, "token", token, token_bytes);

	// nodes
	if (nlen > 0)
	{
		n += snprintf((char*)buffer + n, bytes - n, "5:nodes%u:", (unsigned int)nlen);
		for (i = 0; i < count; i++)
		{
			memcpy(buffer + n, nodes[i]->id, 20);
			n += 20 + sockaddr_write(buffer + n + 20, &nodes[i]->addr);
		}
	}

	// nodes
	if (n6len > 0)
	{
		n += snprintf((char*)buffer + n, bytes - n, "6:nodes6%u:", (unsigned int)n6len);
		for (i = 0; i < count; i++)
		{
			memcpy(buffer + n, nodes[i]->id, 20);
			n += 20 + sockaddr_write(buffer + n + 20, &nodes[i]->addr);
		}
	}

	// values
	if (peernum > 0)
	{
		n += snprintf((char*)buffer + n, bytes - n, "6:valuesl");
		for (i = 0; i < peernum; i++)
		{
			if (AF_INET == peers[i].ss_family)
			{
				buffer[n++] = '6';
			}
			else
			{
				buffer[n++] = '1';
				buffer[n++] = '8';
			}
			buffer[n++] = ':';
			n += sockaddr_write(buffer + n, peers + i);
		}

		buffer[n++] = 'e'; // end of values list
	}

	memcpy(buffer + n, "ee", 2);
	return n + 2;
}

int dht_announce_peer_write(uint8_t* buffer, uint32_t bytes, const uint16_t transaction, const uint8_t id[20], const uint8_t info_hash[20], uint16_t port, const uint8_t* token, uint32_t token_bytes)
{
	int n;

	// announce_peers Query = {"t":"aa", "y":"q", "q":"announce_peer", "a": {"id":"abcdefghij0123456789", "implied_port": 1, "info_hash":"mnopqrstuvwxyz123456", "port": 6881, "token": "aoeusnth"}}
	n = snprintf((char*)buffer, bytes, "d1:t3:%c--1:y1:q1:q13:announce_peer1:ad2:id20:", DHT_ANNOUNCE_PEER);
	if (n < 0 || n + 2 + 20 + 34/*info_hash*/ + token_bytes + 16 + 18 /*port*/ >= bytes)
		return -1;
	buffer[7] = (uint8_t)(transaction >> 8);
	buffer[8] = (uint8_t)transaction;
	memcpy(buffer + n, id, 20);
	n += 20;

	n += bencode_write_item(buffer + n, bytes - n, "info_hash", info_hash, 20);
	n += bencode_write_item(buffer + n, bytes - n, "token", token, token_bytes);
	if (0 == port)
		n += snprintf((char*)buffer + n, bytes - n, "12:implied_porti1e");
	else
		n += snprintf((char*)buffer + n, bytes - n, "4:porti%hue", port);
	memcpy(buffer + n, "ee", 2);
	return n + 2;
}

int dht_announce_peer_reply_write(uint8_t* buffer, uint32_t bytes, const uint8_t* transaction, uint32_t tidbytes, const uint8_t id[20])
{
	int n;

	// Response = {"t":"aa", "y":"r", "r": {"id":"mnopqrstuvwxyz123456"}}
	n = snprintf((char*)buffer, bytes, "d1:t%u:", (unsigned int)tidbytes);
	if (n < 0 || n + tidbytes + 20 + 20 > bytes)
		return -1;
	memcpy(buffer + n, transaction, tidbytes);
	n += tidbytes;

	n += snprintf((char*)buffer + n, bytes - n, "1:y1:r1:rd2:id20:");
	memcpy(buffer + n, id, 20);
	memcpy(buffer + n + 20, "ee", 2);
	return n + 2 + 20;
}

int dht_message_read(const uint8_t* buffer, uint32_t bytes, struct dht_message_handler_t* handler, void* param)
{
	int ret = -1;
	struct bvalue_t root;
	const struct bvalue_t *t, *y, *id;
	const struct bvalue_t *v, *ip, *readonly;
	if (bencode_read(buffer, bytes, &root) <= 0)
		return -1;

	y = bencode_find(&root, "y");
	t = bencode_find(&root, "t");
	v = bencode_find(&root, "v"); // version
	ip = bencode_find(&root, "ip"); // external ip
	readonly = bencode_find(&root, "ro"); // read only
	if (!y || BT_STRING != y->type || !t || BT_STRING != t->type || t->v.str.bytes < 1)
	{
		bencode_free(&root);
		return -1;
	}

	if ('q' == y->v.str.value[0])
	{
		const struct bvalue_t *q, *a;
		q = bencode_find(&root, "q");
		a = bencode_find(&root, "a");

		if (!q || BT_STRING != q->type || !a || BT_DICT != a->type)
		{
			bencode_free(&root);
			return -1;
		}

		id = bencode_find(a, "id");
		if (!id || BT_STRING != id->type || 20 != id->v.str.bytes)
		{
			bencode_free(&root);
			return -1;
		}

		if (0 == strcmp("ping", q->v.str.value))
		{
			ret = handler->ping(param, (uint8_t*)t->v.str.value, t->v.str.bytes, (uint8_t*)id->v.str.value);
		}
		else if (0 == strcmp("find_node", q->v.str.value))
		{
			const struct bvalue_t *target;
			target = bencode_find(a, "target");
			if (!target || BT_STRING != target->type || 20 != target->v.str.bytes)
			{
				bencode_free(&root);
				return -1;
			}

			ret = handler->find_node(param, (uint8_t*)t->v.str.value, t->v.str.bytes, (uint8_t*)id->v.str.value, (uint8_t*)target->v.str.value);
		}
		else if (0 == strcmp("get_peers", q->v.str.value))
		{
			const struct bvalue_t *info_hash;
			const struct bvalue_t *name, *scrape, *noseed;
			info_hash = bencode_find(a, "info_hash");
			name = bencode_find(a, "name"); // string
			scrape = bencode_find(a, "scrape"); // int
			noseed = bencode_find(a, "noseed"); // int
			if (!info_hash || BT_STRING != info_hash->type || 20 != info_hash->v.str.bytes)
			{
				bencode_free(&root);
				return -1;
			}

			ret = handler->get_peers(param, (uint8_t*)t->v.str.value, t->v.str.bytes, (uint8_t*)id->v.str.value, (uint8_t*)info_hash->v.str.value);
		}
		else if (0 == strcmp("announce_peer", q->v.str.value))
		{
			const struct bvalue_t *info_hash, *port, *implied_port, *token, *seed;
			implied_port = bencode_find(a, "implied_port");
			info_hash = bencode_find(a, "info_hash");
			port = bencode_find(a, "port");
			seed = bencode_find(a, "seed"); // int
			token = bencode_find(a, "token");
			if (!info_hash || BT_STRING != info_hash->type || 20 != info_hash->v.str.bytes
				|| !token || BT_STRING != token->type || token->v.str.bytes < 1
				|| ((!port || BT_INT != port->type) && (!implied_port || BT_INT != implied_port->type)) )
			{
				bencode_free(&root);
				return -1;
			}

			ret = handler->announce_peer(param, (uint8_t*)t->v.str.value, t->v.str.bytes, (uint8_t*)id->v.str.value, (uint8_t*)info_hash->v.str.value, implied_port ? 0 : (uint16_t)port->v.value, (uint8_t*)token->v.str.value, token->v.str.bytes);
		}
		else if (0 == strcmp("vote", q->v.str.value))
		{
			const struct bvalue_t *target, *token, *name, *vote;
			target = bencode_find(a, "target");
			token = bencode_find(a, "token"); // string
			name = bencode_find(a, "name"); // string
			vote = bencode_find(a, "vote"); // int
			if (!target || BT_STRING != target->type || 20 != target->v.str.bytes
				|| !token || BT_STRING != token->type
				|| !name || BT_STRING != name->type
				|| !vote || BT_INT != vote->type)
			{
				bencode_free(&root);
				return -1;
			}
			ret = 0;
		}
		else if (0 == strcmp("get", q->v.str.value))
		{
			const struct bvalue_t *target, *seq;
			target = bencode_find(a, "target");
			seq = bencode_find(a, "seq"); // int64
			if (!target || BT_STRING != target->type || 20 != target->v.str.bytes
				|| !seq || BT_INT != seq->type)
			{
				bencode_free(&root);
				return -1;
			}
			ret = 0;
		}
		else if (0 == strcmp("set", q->v.str.value))
		{
			const struct bvalue_t *target, *sig, *seq;
			const struct bvalue_t *k, *salt, *cas;
			target = bencode_find(a, "target");
			sig = bencode_find(a, "sig");
			seq = bencode_find(a, "seq"); // int64
			k = bencode_find(a, "k");
			salt = bencode_find(a, "salt");
			cas = bencode_find(a, "cas"); // int
			if (!target || BT_STRING != target->type || 20 != target->v.str.bytes
				|| !sig || BT_STRING != sig->type || 64 != sig->v.str.bytes
				|| !k || BT_STRING != k->type || 32 != k->v.str.bytes
				|| !seq || BT_INT != seq->type
				|| !salt || BT_STRING != salt->type || salt->v.str.bytes > 64)
			{
				bencode_free(&root);
				return -1;
			}
			ret = 0;
		}
		else if (0 == strcmp("punch", q->v.str.value))
		{
			const struct bvalue_t *addr;
			addr = bencode_find(a, "ip");
			if (!addr || BT_STRING != addr->type || 6 != addr->v.str.bytes)
			{
				bencode_free(&root);
				return -1;
			}
			ret = 0;
		}
		else
		{
			assert(0);
			ret = 0; // unknown command
		}
	}
	else if ('r' == y->v.str.value[0])
	{
		uint16_t transaction;
		const struct bvalue_t *r;
		r = bencode_find(&root, "r");
		if (!r || BT_DICT != r->type || 3 != t->v.str.bytes)
		{
			bencode_free(&root);
			return -1;
		}

		id = bencode_find(r, "id");
		if (!id || BT_STRING != id->type || 20 != id->v.str.bytes)
		{
			bencode_free(&root);
			return -1;
		}

		transaction = (t->v.str.value[1] << 8) | t->v.str.value[2];
		switch(t->v.str.value[0])
		{
		case DHT_PING:
			ret = handler->pong(param, 0, transaction, (uint8_t*)id->v.str.value);
			break;

		case DHT_FIND_NODE:
			ret = dht_message_read_find_node_reply(r, id, transaction, handler, param);
			break;

		case DHT_GET_PEERS:
			ret = dht_message_read_get_peers_reply(r, id, transaction, handler, param);
			break;

		case DHT_ANNOUNCE_PEER:
			ret = handler->announce_peer_reply(param, 0, transaction, (uint8_t*)id->v.str.value);
			break;

		case DHT_VOTE:
		case DHT_PUT:
		case DHT_GET:
		case DHT_PUNCH:
		default:
			assert(0);
			ret = -1; // unknown transaction id
		}
	}
	else if ('e' == y->v.str.value[0])
	{
		uint16_t transaction;
		const struct bvalue_t *e, *msg;
		e = bencode_find(&root, "e");
		if (!e || BT_LIST != e->type || e->v.list.count < 2 || 3 != t->v.str.bytes)
		{
			bencode_free(&root);
			return -1;
		}

		id = &e->v.list.values[0];
		msg = &e->v.list.values[1];
		transaction = (t->v.str.value[1] << 8) | t->v.str.value[2];
		if (BT_INT != id->type || BT_STRING != msg->type)
		{
			bencode_free(&root);
			return -1;
		}

		switch(t->v.str.value[0])
		{
		case DHT_PING:
			ret = handler->pong(param, (int)id->v.value, transaction, NULL);
			break;

		case DHT_FIND_NODE:
			ret = handler->find_node_reply(param, (int)id->v.value, transaction, NULL, NULL, 0);
			break;

		case DHT_GET_PEERS:
			ret = handler->get_peers_reply(param, (int)id->v.value, transaction, NULL, NULL, 0, NULL, 0, NULL, 0);
			break;
			
		case DHT_ANNOUNCE_PEER:
			ret = handler->announce_peer_reply(param, (int)id->v.value, transaction, NULL);
			break;

		case DHT_VOTE:
		case DHT_GET:
		case DHT_PUT:
		case DHT_PUNCH:
		default:
			assert(0);
			ret = -1; // unknown transaction id
		}
	}
	else
	{
		assert(0);
		ret = -1; // unknown command
	}

	bencode_free(&root);
	return ret;
}

static int dht_message_read_find_node_reply(const struct bvalue_t *r, const struct bvalue_t *id, uint16_t transaction, struct dht_message_handler_t* handler, void* param)
{
	int ret;
	size_t i, n;
	struct node_t* items;
	const struct bvalue_t *nodes, *nodes6;
	nodes = bencode_find(r, "nodes");
	nodes6 = bencode_find(r, "nodes6");
	if ( (!nodes || BT_STRING != nodes->type || nodes->v.str.bytes < 26) 
		&& (!nodes6 || BT_STRING != nodes6->type || nodes->v.str.bytes < 38) )
		return -1;

	items = calloc( (nodes ? nodes->v.str.bytes / 26 : 0) + (nodes6 ? nodes6->v.str.bytes / 38 : 0), sizeof(struct node_t));
	if (!items)
		return -1;

	n = 0;
	if (nodes)
	{
		assert(0 == nodes->v.str.bytes % 26);
		for (i = 0; i < nodes->v.str.bytes / 26; i++, n++)
		{
			items[n].ref = 1;
			memcpy(items[n].id, nodes->v.str.value + i * 26, sizeof(items[n].id));
			sockaddr_read(&items[n].addr, (const uint8_t*)nodes->v.str.value + i * 26 + 20, 0);
		}
	}

	if (nodes6)
	{
		assert(0 == nodes6->v.str.bytes % 38);
		for (i = 0; i < nodes6->v.str.bytes / 38; i++, n++)
		{
			items[n].ref = 1;
			memcpy(items[n].id, nodes6->v.str.value + i * 38, sizeof(items[n].id));
			sockaddr_read(&items[n].addr, (const uint8_t*)nodes->v.str.value + i * 38 + 20, 1);
		}
	}

	ret = handler->find_node_reply(param, 0, transaction, (uint8_t*)id->v.str.value, items, n);
	free(items);
	return ret;
}

static int dht_message_read_get_peers_reply(const struct bvalue_t *r, const struct bvalue_t *id, uint16_t transaction, struct dht_message_handler_t* handler, void* param)
{
	int ret;
	size_t i, n, m;
	struct node_t* items = NULL;
	struct sockaddr_storage *peers = NULL;
	const struct bvalue_t *token, *values;
	const struct bvalue_t *nodes, *nodes6;
	token = bencode_find(r, "token");
	nodes = bencode_find(r, "nodes");
	nodes6 = bencode_find(r, "nodes6");
	values = bencode_find(r, "values");

	// don't have token keyword if don't find peers
	//if (!token || BT_STRING != token->type || token->v.str.bytes < 1)
	//	return -1;

	if ((!nodes || BT_STRING != nodes->type || nodes->v.str.bytes < 26) && (!nodes6 || BT_STRING != nodes6->type || nodes->v.str.bytes < 38) 
		&& (!values || BT_LIST != values->type))
	{
		return -1;
	}

	if (nodes || nodes6)
	{
		items = calloc((nodes ? nodes->v.str.bytes / 26 : 0) + (nodes6 ? nodes6->v.str.bytes / 38 : 0), sizeof(struct node_t));
		if (!items)
			return -1;
	}

	n = 0;
	if (nodes)
	{
		assert(0 == nodes->v.str.bytes % 26);
		for (i = 0; i < nodes->v.str.bytes / 26; i++, n++)
		{
			items[n].ref = 1;
			memcpy(items[n].id, nodes->v.str.value + i * 26, sizeof(items[n].id));
			sockaddr_read(&items[n].addr, (const uint8_t*)nodes->v.str.value + i * 26 + 20, 0);
		}
	}

	if (nodes6)
	{
		assert(0 == nodes6->v.str.bytes % 38);
		for (i = 0; i < nodes6->v.str.bytes / 38; i++, n++)
		{
			items[n].ref = 1;
			memcpy(items[n].id, nodes6->v.str.value + i * 38, sizeof(items[n].id));
			sockaddr_read(&items[n].addr, (const uint8_t*)nodes->v.str.value + i * 38 + 20, 1);
		}
	}

	m = 0;
	if (values && values->v.list.count > 0)
	{
		peers = calloc(values->v.list.count, sizeof(struct sockaddr_storage));
		if (!peers)
		{
			if (items) free(items);
			return -1;
		}

		for (i = 0; i < values->v.list.count; i++)
		{
			if (BT_STRING != values->v.list.values[i].type || (6 != values->v.list.values[i].v.str.bytes && 18 != values->v.list.values[i].v.str.bytes))
			{
				assert(0);
				continue;
			}
			sockaddr_read(&peers[m++], (const uint8_t*)values->v.list.values[i].v.str.value, 18 == values->v.list.values[i].v.str.bytes ? 1 : 0);
		}
	}

	ret = handler->get_peers_reply(param, 0, transaction, (uint8_t*)id->v.str.value, token ? (uint8_t*)token->v.str.value : NULL, token ? token->v.str.bytes : 0, items, n, peers, m);

	if (items) free(items);
	if (peers) free(peers);
	return ret;
}

static int bencode_write_item(uint8_t* buffer, uint32_t bytes, const char* key, const uint8_t* value, uint32_t size)
{
	int n;
	n = snprintf((char*)buffer, bytes, "%u:%s%u:", (unsigned int)strlen(key), key, size);
	if (n < 0 || n + size > bytes)
		return 0;

	memcpy(buffer + n, value, size);
	return n + size;
}

static int sockaddr_write(uint8_t* buffer, const struct sockaddr_storage* addr)
{
	if (AF_INET == addr->ss_family)
	{
		struct sockaddr_in* ipv4;
		ipv4 = (struct sockaddr_in*)addr;
		memcpy(buffer, &ipv4->sin_addr.s_addr, 4);
		memcpy(buffer + 4, &ipv4->sin_port, 2);
		return 6;
	}
	else if (AF_INET6 == addr->ss_family)
	{
		struct sockaddr_in6* ipv6;
		ipv6 = (struct sockaddr_in6*)addr;
		memcpy(buffer, &ipv6->sin6_addr.s6_addr, 16);
		memcpy(buffer + 16, &ipv6->sin6_port, 2);
		return 18;
	}
	else
	{
		assert(0);
		return 0;
	}
}

static const uint8_t* sockaddr_read(struct sockaddr_storage* addr, const uint8_t* buffer, int ipv6)
{
	memset(addr, 0, sizeof(*addr));
	if (ipv6)
	{
		struct sockaddr_in6* in6;
		in6 = (struct sockaddr_in6*)addr;
		in6->sin6_family = AF_INET6;
		memcpy(&in6->sin6_addr.s6_addr, buffer, 16);
		memcpy(&in6->sin6_port, buffer + 16, 2);
		buffer += 18;
	}
	else
	{
		struct sockaddr_in* ipv4;
		ipv4 = (struct sockaddr_in*)addr;
		ipv4->sin_family = AF_INET;
		memcpy(&ipv4->sin_addr.s_addr, buffer, 4);
		memcpy(&ipv4->sin_port, buffer + 4, 2);
		buffer += 6;
	}

	return buffer;
}

#if defined(DEBUG) || defined(_DEBUG)
void dht_message_test(void)
{
	uint8_t buffer[256];
	const uint8_t ping[] = "d1:t3:p001:y1:q1:q4:ping1:ad2:id20:00000000000000000000ee";
	const uint8_t pong[] = "d1:t3:p001:y1:r1:rd2:id20:99999999999999999999ee";
	const uint8_t find_node[] = "d1:t3:f001:y1:q1:q9:find_node1:ad2:id20:000000000000000000006:target20:xxxxxxxxxxxxxxxxxxxxee";
	const uint8_t find_node_reply[] = "d1:t3:f001:y1:r1:rd2:id20:999999999999999999995:nodes104:11111111111111111111111111222222222222222222222222223333333333333333333333333344444444444444444444444444ee";
	const uint8_t get_peers[] = "d1:t3:g001:y1:q1:q9:get_peers1:ad2:id20:000000000000000000009:info_hash20:xxxxxxxxxxxxxxxxxxxxee";
	const uint8_t get_peers_reply[] = "d1:t3:g001:y1:r1:rd2:id20:999999999999999999995:token6:secret5:nodes104:111111111111111111111111112222222222222222222222222233333333333333333333333333444444444444444444444444446:valuesl6:5555556:666666eee";
	const uint8_t announce_peer[] = "d1:t3:a001:y1:q1:q13:announce_peer1:ad2:id20:000000000000000000009:info_hash20:xxxxxxxxxxxxxxxxxxxx5:token6:secret4:porti65535eee";
	const uint8_t announce_peer_reply[] = "d1:t3:a001:y1:r1:rd2:id20:99999999999999999999ee";
	struct node_t nodes[] = {
		{ 1, "11111111111111111111", { AF_INET, "111111" } },
		{ 1, "22222222222222222222", { AF_INET, "222222" } },
		{ 1, "33333333333333333333", { AF_INET, "333333" } },
		{ 1, "44444444444444444444", { AF_INET, "444444" } },
	};
	struct node_t* nodes2[4];
	const struct sockaddr_storage peers[] = {
		{ AF_INET, "555555" },
		{ AF_INET, "666666" },
	};

	nodes2[0] = &nodes[0]; nodes2[1] = &nodes[1]; nodes2[2] = &nodes[2]; nodes2[3] = &nodes[3];

	assert(sizeof(ping) == dht_ping_write(buffer, sizeof(buffer), 0x3030, (const uint8_t*)"00000000000000000000") + 1);
	assert(0 == memcmp(buffer, ping, sizeof(ping) - 1));
	assert(sizeof(pong) == dht_pong_write(buffer, sizeof(buffer), (const uint8_t*)"p00", 3, (const uint8_t*)"99999999999999999999") + 1);
	assert(0 == memcmp(buffer, pong, sizeof(pong) - 1));

	assert(sizeof(find_node) == dht_find_node_write(buffer, sizeof(buffer), 0x3030, (const uint8_t*)"00000000000000000000", (const uint8_t*)"xxxxxxxxxxxxxxxxxxxx") + 1);
	assert(0 == memcmp(buffer, find_node, sizeof(find_node) - 1));
	assert(sizeof(find_node_reply) == dht_find_node_reply_write(buffer, sizeof(buffer), (const uint8_t*)"f00", 3, (const uint8_t*)"99999999999999999999", nodes2, sizeof(nodes)/sizeof(nodes[0])) + 1);
	assert(0 == memcmp(buffer, find_node_reply, sizeof(find_node_reply) - 1));

	assert(sizeof(get_peers) == dht_get_peers_write(buffer, sizeof(buffer), 0x3030, (const uint8_t*)"00000000000000000000", (const uint8_t*)"xxxxxxxxxxxxxxxxxxxx") + 1);
	assert(0 == memcmp(buffer, get_peers, sizeof(get_peers) - 1));
	assert(sizeof(get_peers_reply) == dht_get_peers_reply_write(buffer, sizeof(buffer), (const uint8_t*)"g00", 3, (const uint8_t*)"99999999999999999999", (const uint8_t*)"secret", 6, nodes2, sizeof(nodes) / sizeof(nodes[0]), peers, sizeof(peers) / sizeof(peers[0])) + 1);
	assert(0 == memcmp(buffer, get_peers_reply, sizeof(get_peers_reply) - 1));

	assert(sizeof(announce_peer) == dht_announce_peer_write(buffer, sizeof(buffer), 0x3030, (const uint8_t*)"00000000000000000000", (const uint8_t*)"xxxxxxxxxxxxxxxxxxxx", 65535, (const uint8_t*)"secret", 6) + 1);
	assert(0 == memcmp(buffer, announce_peer, sizeof(announce_peer) - 1));
	assert(sizeof(announce_peer_reply) == dht_announce_peer_reply_write(buffer, sizeof(buffer), (const uint8_t*)"a00", 3, (const uint8_t*)"99999999999999999999") + 1);
	assert(0 == memcmp(buffer, announce_peer_reply, sizeof(announce_peer_reply) - 1));
}
#endif
