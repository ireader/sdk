#include "router.h"
#include "rbtree.h"
#include "bitmap.h"
#include "heap.h"
#include "sys/locker.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define N_BITS (N_NODEID * 8)

struct rbitem_t
{
	struct rbtree_node_t link;
	struct node_t* node;
};

struct router_t
{
	uint8_t id[N_NODEID];
	locker_t locker;

	size_t count;
	struct rbtree_root_t rbtree;
};

static void rbtree_destroy(struct rbtree_node_t* node);
static int rbtree_find(struct rbtree_root_t* root, const uint8_t id[N_NODEID], struct rbtree_node_t** node);

struct router_t* router_create(const uint8_t id[N_NODEID])
{
	struct router_t* router;
	router = (struct router_t*)calloc(1, sizeof(*router));
	if (!router) return NULL;

	locker_create(&router->locker);
	memcpy(router->id, id, sizeof(router->id));
	router->rbtree.node = NULL;
	return router;
}

void router_destroy(struct router_t* router)
{
	if (router->rbtree.node)
		rbtree_destroy(router->rbtree.node);
	locker_destroy(&router->locker);
	free(router);
}

int router_add(struct router_t* router, const uint8_t id[N_NODEID], const struct sockaddr_storage* addr, struct node_t** node)
{
	int r;
	struct rbitem_t* item;
	struct rbtree_node_t **link;
	struct rbtree_node_t *parent;

	if (node) *node = NULL;

	item = calloc(1, sizeof(*item));
	if (!item)
		return ENOMEM;
	item->node = node_create2(id, addr);
	if (!item->node)
	{
		free(item->node);
		return ENOMEM;
	}

	locker_lock(&router->locker);
	r = rbtree_find(&router->rbtree, id, &parent);
	if (0 == r)
	{
		if (node)
		{
			*node = (rbtree_entry(parent, struct rbitem_t, link))->node;
			node_addref(*node);
		}
		locker_unlock(&router->locker);
		node_release(item->node);
		free(item);
		return EEXIST;
	}
	link = parent ? (r > 0 ? &parent->left : &parent->right) : NULL;
	assert(!link || !*link);

	rbtree_insert(&router->rbtree, parent, link, &item->link);
	router->count += 1;
	
	if (node)
	{
		node_addref(item->node);
		*node = item->node;
	}
	locker_unlock(&router->locker);
	return 0;
}

int router_remove(struct router_t* router, const uint8_t id[N_NODEID])
{
	int r;
	struct rbitem_t* item;
	struct rbtree_node_t* node;

	locker_lock(&router->locker);
	r = rbtree_find(&router->rbtree, id, &node);
	if (0 != r)
	{
		locker_unlock(&router->locker);
		return ENOENT;
	}
	
	router->count -= 1;
	item = rbtree_entry(node, struct rbitem_t, link);
	rbtree_delete(&router->rbtree, node);
	locker_unlock(&router->locker);
	node_release(item->node);
	free(item);
	return 0;
}

size_t router_size(struct router_t* router)
{
	return router->count;
}

int router_nearest(struct router_t* router, const uint8_t id[N_NODEID], struct node_t* nodes[], size_t count)
{
	int i, min, diff;
	uint8_t xor[N_NODEID];
	heap_t* heap;
	struct rbitem_t* item;
	struct rbtree_node_t* node;
	const struct rbtree_node_t* prev;
	const struct rbtree_node_t* next;

	heap = heap_create(node_compare_less, (void*)id);
	heap_reserve(heap, count + 1);

	min = N_BITS;
	locker_lock(&router->locker);
	rbtree_find(&router->rbtree, id, &node);
	if (NULL == node)
	{
		locker_unlock(&router->locker);
		return 0;
	}

	item = rbtree_entry(node, struct rbitem_t, link);
	bitmap_xor(xor, id, item->node->id, N_BITS);
	diff = bitmap_count_leading_zero(xor, N_BITS);
	min = min < diff ? min : diff;
	heap_push(heap, item->node);

	prev = rbtree_prev(node);
	next = rbtree_next(node);
	do
	{
		while (prev)
		{
			item = rbtree_entry(prev, struct rbitem_t, link);
			bitmap_xor(xor, id, item->node->id, N_BITS);
			diff = bitmap_count_leading_zero(xor, N_BITS);
			heap_push(heap, item->node);
			if (heap_size(heap) > (int)count)
				heap_pop(heap);

			prev = rbtree_prev(prev);
			if (diff < min)
			{
				min = diff;
				break; // try right
			}
		}

		while (next)
		{
			item = rbtree_entry(next, struct rbitem_t, link);
			bitmap_xor(xor, id, item->node->id, N_BITS);
			diff = bitmap_count_leading_zero(xor, N_BITS);
			heap_push(heap, item->node);
			if (heap_size(heap) > (int)count)
				heap_pop(heap);

			next = rbtree_next(next);
			if (diff < min)
			{
				min = diff;
				break; // try left
			}
		}
	} while (heap_size(heap) < (int)count && (prev || next));

	for (i = 0; i < (int)count && !heap_empty(heap); i++)
	{
		nodes[i] = heap_top(heap);
		node_addref(nodes[i]);
		heap_pop(heap);
	}

	locker_unlock(&router->locker);
	heap_destroy(heap);
	return i;
}

int router_list(struct router_t* router, int (*func)(void* param, struct node_t* node), void* param)
{
	int r = 0;
	struct rbitem_t* item;
	const struct rbtree_node_t* node;
	
	locker_lock(&router->locker);
	node = rbtree_first(&router->rbtree);
	while (node && 0 == r)
	{
		item = rbtree_entry(node, struct rbitem_t, link);
		r = func(param, item->node);
		node = rbtree_next(node);
	}
	locker_unlock(&router->locker);
	return r;
}

static void rbtree_destroy(struct rbtree_node_t* node)
{
	struct rbitem_t* item;
	item = rbtree_entry(node, struct rbitem_t, link);

	if (node->left)
		rbtree_destroy(node->left);
	if (node->right)
		rbtree_destroy(node->right);

	node_release(item->node);
	free(item);
}

static int rbtree_find(struct rbtree_root_t* root, const uint8_t id[N_NODEID], struct rbtree_node_t** node)
{
	int r;
	struct rbitem_t* item;
	struct rbtree_node_t* link;

	r = -1;
	*node = NULL;
	link = root->node;
	while (link)
	{
		*node = link;
		item = rbtree_entry(link, struct rbitem_t, link);
		r = memcmp(item->node->id, id, N_NODEID);
		if (0 == r)
			break;

		link = r > 0 ? link->left : link->right;
	}

	return r;
}

#if defined(_DEBUG) || defined(DEBUG)
static void router_test2(void)
{
	int i;
	struct node_t* node;
	struct node_t* result[8];
	static struct node_t s_nodes[100000];
	uint8_t id[N_NODEID] = { 0xAB, 0xCD, 0xEF, 0x89, };

	heap_t* heap, *heap2;
	struct router_t* router;
	router = router_create(id);

	heap = heap_create(node_compare_less, (void*)id);
	heap_reserve(heap, 8 + 1);

	for (i = 0; i < sizeof(s_nodes) / sizeof(s_nodes[0]); i++)
	{
		int v = rand();

		memset(&s_nodes[i], 0, sizeof(s_nodes[i]));
		memcpy(s_nodes[i].id, &v, sizeof(v));
		s_nodes[i].ref = 1;

		if (0 == router_add(router, s_nodes[i].id, &s_nodes[i].addr, &node))
		{
			heap_push(heap, node);
			if (heap_size(heap) > 8)
			{
				node_release((struct node_t*)heap_top(heap));
				heap_pop(heap);
			}
		}
	}

	assert(8 == heap_size(heap));
	assert(8 == router_nearest(router, id, result, 8));

	heap2 = heap_create(node_compare_less, (void*)id);
	heap_reserve(heap2, 8);

	for (i = 0; i < 8; i++)
	{
		heap_push(heap2, result[i]);
	}

	assert(heap_size(heap) == heap_size(heap2));
	for (i = 0; i < 8; i++)
	{
		assert(heap_top(heap2) == heap_top(heap));
		heap_pop(heap);
		heap_pop(heap2);
	}

	router_destroy(router);
	heap_destroy(heap);
	heap_destroy(heap2);
	printf("router test ok!\n");
}

void router_test(void)
{
	int i;
	uint8_t id[N_NODEID] = {0x56, 0x31,};
	struct node_t* nodes2[10];
	struct node_t nodes[] = {
		{ 1,{ 0x56, 0x25, } },
		{ 1,{ 0x56, 0x80, } },
		{ 1,{ 0x56, 0xFF, } },
		{ 1,{ 0x56, 0x00, } },
		{ 1,{ 0x56, 0x71, } },
		{ 1,{ 0x56, 0x10, } },
		{ 1,{ 0x56, 0x0E, } },
		{ 1,{ 0x56, 0xCE, } },

		{ 1,{ 0x7E, 0x31, } },
		{ 1,{ 0x29, 0x31, } },
		{ 1,{ 0x80, 0x31, } },
		{ 1,{ 0x69, 0x31, } },
		{ 1,{ 0x59, 0x31, } },
		{ 1,{ 0x40, 0x31, } },
	};

	struct router_t* router;
	router = router_create(id);
	for (i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++)
	{
		router_add(router, nodes[i].id, &nodes[i].addr, NULL);
	}
	router_nearest(router, id, nodes2, 6);
	router_destroy(router);

	while(1)
		router_test2();
}
#endif
