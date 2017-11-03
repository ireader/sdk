#include "router.h"
#include "rbtree.h"
#include "bitmap.h"
#include "heap.h"
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

	memcpy(router->id, id, sizeof(router->id));
	router->rbtree.node = NULL;
	return router;
}

void router_destroy(struct router_t* router)
{
	if (router->rbtree.node)
		rbtree_destroy(router->rbtree.node);
	free(router);
}

int router_add(struct router_t* router, struct node_t* node)
{
	int r;
	struct rbitem_t* item;
	struct rbtree_node_t **link;
	struct rbtree_node_t *parent;

	r = rbtree_find(&router->rbtree, node->id, &parent);
	if (0 == r)
		return EEXIST;
	link = parent ? (r > 0 ? &parent->left : &parent->right) : NULL;
	assert(!link || !*link);

	item = calloc(1, sizeof(*item));
	if (!item)
		return ENOMEM;
	
	node_addref(node);
	item->node = node;
	rbtree_insert(&router->rbtree, parent, link, &item->link);
	router->count += 1;
	return 0;
}

int router_remove(struct router_t* router, const uint8_t id[N_NODEID])
{
	int r;
	struct rbitem_t* item;
	struct rbtree_node_t* node;

	r = rbtree_find(&router->rbtree, id, &node);
	if (0 != r)
		return ENOENT;
	
	router->count -= 1;
	item = rbtree_entry(node, struct rbitem_t, link);
	rbtree_delete(&router->rbtree, node);
	node_release(item->node);
	free(item);
	return 0;
}

size_t router_size(struct router_t* router)
{
	return router->count;
}

static int heap_compare(void* param, const void* l, const void* r)
{
	uint8_t xor1[N_NODEID], xor2[N_NODEID];
	bitmap_xor(xor1, (const uint8_t*)param, ((const struct node_t*)l)->id, N_BITS);
	bitmap_xor(xor2, (const uint8_t*)param, ((const struct node_t*)r)->id, N_BITS);
	return memcmp(xor1, xor2, N_NODEID) > 0 ? 1 : 0;
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

	heap = heap_create(heap_compare, (void*)id);
	heap_reserve(heap, count + 1);

	min = N_BITS;
	rbtree_find(&router->rbtree, id, &node);
	if (NULL == node)
		return 0;

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
	} while (heap_size(heap) < (int)count);

	for (i = 0; i < (int)count && !heap_empty(heap); i++)
	{
		nodes[i] = heap_top(heap);
		heap_pop(heap);
	}
	heap_destroy(heap);
	return i;
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
	int i, j;
	struct node_t* result[8];
	struct node_t* result2[8];
	static struct node_t s_nodes[100000];
	uint8_t id[N_NODEID] = { 0xAB, 0xCD, 0xEF, 0x89, };
	uint8_t xor[N_NODEID];

	heap_t* heap, *heap2;
	struct router_t* router;
	router = router_create(id);

	heap = heap_create(heap_compare, (void*)id);
	heap_reserve(heap, 8 + 1);

	for (i = 0; i < sizeof(s_nodes) / sizeof(s_nodes[0]); i++)
	{
		int v = rand();

		memset(&s_nodes[i], 0, sizeof(s_nodes[i]));
		memcpy(s_nodes[i].id, &v, sizeof(v));
		s_nodes[i].ref = 1;

		if (0 == router_add(router, &s_nodes[i]))
		{
			heap_push(heap, &s_nodes[i]);
			if (heap_size(heap) > 8)
				heap_pop(heap);
		}
	}

	assert(8 == heap_size(heap));
	assert(8 == router_nearest(router, id, result, 8));

	heap2 = heap_create(heap_compare, (void*)id);
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
		router_add(router, nodes + i);
	}
	router_nearest(router, id, nodes2, 6);
	router_destroy(router);

	while(1)
		router_test2();
}
#endif
