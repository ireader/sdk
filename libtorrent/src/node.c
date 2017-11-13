#include "node.h"
#include "sys/atomic.h"
#include "bitmap.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct node_t* node_create()
{
	struct node_t* node;
	node = calloc(1, sizeof(*node));
	if (!node) return NULL;

	node->ref = 1;
	node->clock = 0;
	node->active = 0;
	node->status = NODE_STATUS_UNKNOWN;
	return node;
}

struct node_t* node_create2(const uint8_t id[N_NODEID], const struct sockaddr_storage* addr)
{
	struct node_t* node;
	node = node_create();
	if (!node) return NULL;

	memcpy(node->id, id, sizeof(node->id));
	memcpy(&node->addr, addr, sizeof(node->addr));
	return node;
}

int node_settoken(struct node_t* node, const uint8_t* token, unsigned int bytes)
{
	void* ptr;
	if (bytes > node->capacity)
	{
		ptr = realloc(node->token, bytes + 32);
		if (!ptr) return ENOMEM;
		node->token = (uint8_t*)ptr;
		node->capacity = bytes + 32;
	}

	memcpy(node->token, token, bytes);
	node->bytes = bytes;
	return 0;
}

int32_t node_addref(struct node_t* node)
{
	return atomic_increment32(&node->ref);
}

int32_t node_release(struct node_t* node)
{
	int32_t ref;
	ref = atomic_decrement32(&node->ref);
	if (0 == ref)
	{
		if (node->token)
		{
			free(node->token);
			node->token = NULL;
			node->bytes = 0;
		}
		free(node);
	}
	return ref;
}

int node_compare_less(const uint8_t ref[N_NODEID], const struct node_t* l, const struct node_t* r)
{
	uint8_t xor1[N_NODEID], xor2[N_NODEID];
	bitmap_xor(xor1, ref, l->id, N_NODEID * 8);
	bitmap_xor(xor2, ref, r->id, N_NODEID * 8);
	return memcmp(xor1, xor2, N_NODEID) > 0 ? 1 : 0;
}

int node_compare_great(const uint8_t ref[N_NODEID], const struct node_t* l, const struct node_t* r)
{
	return 1 - node_compare_less(ref, l, r);
}
