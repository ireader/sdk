#include "rbtree.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "cstringext.h"

#define N 1000

struct rbtree_value_t
{
	struct rbtree_node_t node;
	int value;
};

static void rbtree_check_node(struct rbtree_node_t* node, int* black)
{
	int i;
	struct rbtree_node_t* parent;

	assert(node);
	if (0 == node->color)
	{
		// red node parent must be black
		assert(node->parent->color);
		// red node child must be null or two black node
		assert((!node->left && !node->right) || (node->left->color && node->right->color));
	}

	if (!node->left || !node->right)
	{
		// check black node count
		i = node->color ? 1 : 0;
		for (parent = node->parent; parent; parent = parent->parent)
		{
			if (parent->color)
				i++;
		}
		assert(0 == *black || i == *black);
		*black = i;
	}

	if (node->left)
		rbtree_check_node(node->left, black);

	if (node->right)
		rbtree_check_node(node->right, black);
}

static void rbtree_validate(struct rbtree_root_t* root)
{
	int black = 0;
	if (root->node)
		rbtree_check_node(root->node, &black);
}

static char s_tree[256];
static void rbtree_print_node(struct rbtree_node_t* node)
{
	struct rbtree_node_t* p;
	struct rbtree_node_t* g;
	struct rbtree_value_t* v;
	if (!node)
		return;

	p = node->parent;
	g = p ? p->parent : NULL;

	s_tree[0] = 0;
	while (g)
	{
		if (p == g->left && g->right)
		{
			strcat(s_tree, "\t|");
		}
		else
		{
			strcat(s_tree, "\t");
		}

		p = g;
		g = p->parent;
	}
	strrev(s_tree);

	v = rbtree_entry(node, struct rbtree_value_t, node);

	if(!node->parent)
		printf("%d(%c)\n", v->value, v->node.color ? 'B' : 'R');
	else
		printf("%s|----%d(%c)\n", s_tree, v->value, v->node.color ? 'B' : 'R');

	rbtree_print_node(node->left);
	rbtree_print_node(node->right);
}

static void rbtree_print(struct rbtree_root_t* root)
{
	rbtree_print_node(root->node);
}

static int rbtree_find(struct rbtree_root_t* root, int v, struct rbtree_node_t** node)
{
	int r;
	struct rbtree_node_t* link;
	struct rbtree_value_t* value;

	r = -1;
	*node = NULL;
	link = root->node;
	while (link)
	{
		*node = link;
		value = rbtree_entry(link, struct rbtree_value_t, node);
		r = value->value - v;
		if (0 == r)
			break;

		link = r > 0 ? link->left : link->right;
	}

	return r;
}

static void rbtree_iter_test(void)
{
	int r, i;
	struct rbtree_root_t root;
	struct rbtree_node_t **link;
	struct rbtree_node_t *parent;
	const struct rbtree_node_t *node;
	struct rbtree_value_t* value;

	root.node = NULL;
	for (i = 0; i < 100000; i++)
	{
		r = rbtree_find(&root, i, &parent);
		assert(0 != r);
		link = parent ? (r > 0 ? &parent->left : &parent->right) : NULL;

		value = (struct rbtree_value_t*)malloc(sizeof(*value));
		value->value = i;
		rbtree_insert(&root, parent, link, &value->node);
	}

	node = rbtree_first(&root);
	for (i = 0; i < 100000; i++)
	{
		value = rbtree_entry(node, struct rbtree_value_t, node);
		assert(i == value->value);
		node = rbtree_next(node);
	}

	node = rbtree_last(&root);
	for (i = 0; i < 100000; i++)
	{
		value = rbtree_entry(node, struct rbtree_value_t, node);
		assert(100000 - 1 - i == value->value);
		node = rbtree_prev(node);
	}
}

void rbtree_test(void)
{
	int i, v[N];
	struct rbtree_root_t root;
	struct rbtree_node_t* parent;
	struct rbtree_node_t** link;
	struct rbtree_value_t* value;

	root.node = NULL;

	int seed = (int)time(NULL); // 1506061127 (duplicate)
	srand(seed);
	for (i = 0; i < N; i++)
	{
		v[i] = rand();
		
		parent = NULL;
		link = &root.node;
		while(*link)
		{
			parent = *link;
			value = rbtree_entry(parent, struct rbtree_value_t, node);
			// duplicate node insert at right children tree
			link = value->value > v[i] ? &parent->left : &parent->right;
		}

		value = (struct rbtree_value_t*)malloc(sizeof(*value));
		value->value = v[i];
		rbtree_insert(&root, parent, link, &value->node);
		rbtree_validate(&root);
	}

//	rbtree_print(&root);

	for (i = 0; i < N; i++)
	{
		int m = rand() % N;
		int n = rand() % N;
		int l = v[m];
		v[m] = v[n];
		v[n] = l;
	}

	for (i = 0; i < N; i++)
	{
		assert(0 == rbtree_find(&root, v[i], &parent));
		value = rbtree_entry(parent, struct rbtree_value_t, node);
		assert(value->value == v[i]);
		rbtree_delete(&root, parent);
		rbtree_validate(&root);
	}

	rbtree_iter_test();
	printf("rb-tree test ok\n");
}
