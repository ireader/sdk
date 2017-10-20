#include "ipaddr-pool.h"
#include "hash-list.h"
#include "jhash.h"
#include "hash.h"
#include <stdlib.h>
#include <errno.h>

#define N_IPADDR_BITS 10

struct ipaddr_pool_t
{
	size_t n;
	struct hash_head_t* head;
};

struct ipaddr_node_t
{
	struct hash_node_t link;
	struct sockaddr_storage addr;
};

ipaddr_pool_t* ipaddr_pool_create()
{
	struct ipaddr_pool_t* pool;
	pool = (struct ipaddr_pool_t*)calloc(1, sizeof(*pool) + sizeof(struct hash_head_t) * (1 << N_IPADDR_BITS));
	if (pool)
	{
		pool->head = (struct hash_head_t*)(pool + 1);
	}
	return pool;
}

void ipaddr_pool_destroy(struct ipaddr_pool_t* pool)
{
	ipaddr_pool_clear(pool);
	free(pool);
}

void ipaddr_pool_clear(struct ipaddr_pool_t* pool)
{
	size_t i, sz = 1UL << N_IPADDR_BITS;
	struct hash_node_t *pos, *n;
	struct ipaddr_node_t* node;

	for (i = 0; i < sz; i++)
	{
		hash_list_for_each_safe(pos, n, pool->head + i)
		{
			node = hash_list_entry(pos, struct ipaddr_node_t, link);
			hash_list_unlink(pos);
			free(node);
		}
	}
}

int ipaddr_pool_empty(struct ipaddr_pool_t* pool)
{
	return !pool->n;
}

size_t ipaddr_pool_size(struct ipaddr_pool_t* pool)
{
	return pool->n;
}

static struct ipaddr_node_t* ipaddr_pool_find(struct ipaddr_pool_t* pool, uint32_t key, const struct sockaddr_storage* addr)
{
	struct hash_node_t* pos;
	struct ipaddr_node_t* node;

	hash_list_for_each(pos, pool->head + key)
	{
		node = hash_list_entry(pos, struct ipaddr_node_t, link);
		if (0 == memcmp(addr, &node->addr, sizeof(node->addr)))
			return node;
	}
	return NULL;
}

int ipaddr_pool_push(struct ipaddr_pool_t* pool, const struct sockaddr_storage* addr)
{
	uint32_t key;
	struct ipaddr_node_t* node;
	
	key = jhash(&addr, sizeof(*addr), ((struct sockaddr_in*)addr)->sin_addr.s_addr);
	key = hash_int32(key, N_IPADDR_BITS);
	if (ipaddr_pool_find(pool, key, addr))
		return EEXIST;

	node = (struct ipaddr_node_t*)malloc(sizeof(*node));
	if (!node) return ENOMEM;

	memcpy(&node->addr, addr, sizeof(node->addr));
	hash_list_link(pool->head + key, &node->link);
	return 0;
}

int ipaddr_pool_pop(struct ipaddr_pool_t* pool, struct sockaddr_storage* addr)
{
	size_t i, sz = 1UL << N_IPADDR_BITS;
	struct ipaddr_node_t* node;

	for (i = 0; i < sz; i++)
	{
		if(!pool->head[i].first)
			continue;
		
		node = hash_list_entry(pool->head[i].first, struct ipaddr_node_t, link);
		memcpy(addr, &node->addr, sizeof(*addr));
		hash_list_unlink(&node->link);
		free(node);
		return 0;
	}

	return ENOENT; // emtpy
}
