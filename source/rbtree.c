// https://en.wikipedia.org/wiki/Red-black_tree
// 1. Each node is either red or black.
// 2. The root is black.
// 3. All leaves (NIL) are black.
// 4. If a node is red, then both its children are black.
// 5. Every path from a given node to any of its descendant NIL nodes contains the same number of black nodes. Some definitions: the number of black nodes from the root to a node is the node's black depth; the uniform number of black nodes in all paths from root to the leaves is called the black-height of the red¨Cblack tree.

#include "rbtree.h"
#include <assert.h>

#define RBT_RED 0
#define RBT_BLACK 1

#define RBT_SENTINEL ((struct rbtree_node_t*)0)

static void rbtree_rotate(struct rbtree_root_t* root, struct rbtree_node_t* node, struct rbtree_node_t* child)
{
	struct rbtree_node_t* parent;

	assert(node && child);
	assert(node->left == child || node->right == child);
	parent = node->parent;

	if (parent)
	{
		if (parent->left == node)
			parent->left = child;
		else
			parent->right = child;
	}
	else
	{
		root->node = child;
	}

	if (node->left == child)
	{
		/* rotate right
		 *			P                   P
		 *		   /                   /
		 *		  N        ->         C
		 *		 / \                 / \
		 *		C   S               L   N
		 *	   / \                     / \
		 *	  L   R                   R   S
		*/
		node->left = child->right;
		if (child->right)
			child->right->parent = node;
		child->right = node;
	}
	else
	{
		/* rotate left
		 *         P                 P
		 *  	  /                 /
		 *		 N        ->       C
		 *		/ \               / \
		 *	   S   C             N   R
		 *	      / \           / \
		 *		 L   R         S   L
		*/
		node->right = child->left;
		if (child->left)
			child->left->parent = node;
		child->left = node;
	}

	node->parent = child;
	child->parent = parent;
}

static void rbtree_insert_color(struct rbtree_root_t* root, struct rbtree_node_t* node)
{
	struct rbtree_node_t* p; // parent
	struct rbtree_node_t* g; // grandparent
	struct rbtree_node_t* u; // uncle (parent sibling) node

	while (1)
	{
		p = node->parent;
		if (!p)
		{
			// node is root
			node->color = RBT_BLACK;
			break;
		}
		else if (p->color == RBT_BLACK)
		{
			break;
		}

		assert(p->parent); // check by parent color is red
		g = p->parent;
		if (p == g->left)
		{
			u = g->right;
			if (u && u->color == RBT_RED)
			{
				/* case 1: lowercase red, uppercase black
				 *		 G                 g (new n)
				 *	    / \               / \
				 *	   p   u      ->     P   U
				 *	  /                 /
				 *   n                 n
				*/
				g->color = RBT_RED;
				p->color = RBT_BLACK;
				u->color = RBT_BLACK;
				node = g;
				continue;
			}
			else
			{
				/* case 2: left rotate, U(black or sentinel)
				 *		G                G
				 *	   / \              / \
				 *	  p   U     ->     n   U
				 *	   \              /
				 *		n            p (new n)
				*/
				if (p->right == node)
				{
					rbtree_rotate(root, p, node);

					node = p;
					p = node->parent;
				}

				/* case 3: right rotate, U(black or sentinel)
				 *		 G                  P
				 *	    / \                / \
				 *	   p   U      ->      n   g
				 *	  /                        \
				 *   n                          U
				*/
				rbtree_rotate(root, g, p);
				p->color = RBT_BLACK;
				g->color = RBT_RED;
				break;
			}
		}
		else
		{
			u = g->left;
			if (u && u->color == RBT_RED)
			{
				// case 1
				g->color = RBT_RED;
				p->color = RBT_BLACK;
				u->color = RBT_BLACK;
				node = g;
				continue;
			}
			else
			{
				if (p->left == node)
				{
					// case 2
					rbtree_rotate(root, p, node);

					node = p;
					p = node->parent;
				}

				// case 3
				rbtree_rotate(root, g, p);
				p->color = RBT_BLACK;
				g->color = RBT_RED;
				break;
			}
		}
	}
}

static void rbtree_delete_color(struct rbtree_root_t* root, struct rbtree_node_t* node)
{
	struct rbtree_node_t* p; // parent
	struct rbtree_node_t* s; // sibling
	struct rbtree_node_t* sl; // sibling left child
	struct rbtree_node_t* sr; // sibling right child

	while (1)
	{
		p = node->parent;
		if (!p)
		{
			// node is root
			assert(node->color == RBT_BLACK);
			break;
		}

		if (p->left == node)
		{
			s = p->right;
			if (s->color == RBT_RED)
			{
				/* case 1: RED sibling node, left rotate P
				 *      P                 s               S
				 *     / \               / \             / \
				 *    N   s      ->     P   SR   ->     p   SR
				 *       / \           / \             / \
				 *      SL  SR        N   SL          N   SL
				*/
				rbtree_rotate(root, p, s);
				s->color = RBT_BLACK;
				p->color = RBT_RED;
				s = p->right;
			}

			assert(s->color == RBT_BLACK);
			sl = s->left;
			sr = s->right;
			if (!sr || sr->color == RBT_BLACK)
			{
				if (!sl || sl->color == RBT_BLACK)
				{
					/* case 2: BLACK sibling node with two black(sentinel) node, ANY parent
					 *      (P)               (P) (new N)
					 *      / \               / \
					 *     N   S      ->     N   s
					 *        / \               / \
					 *       SL  SR            SL  SR
					*/
					s->color = RBT_RED;
					if (p->color == RBT_RED || !p->parent /*root*/)
					{
						p->color = RBT_BLACK;
						break;
					}
					else
					{
						node = p;
						continue;
					}
				}

				if (sl->color == RBT_RED)
				{
					/* case 3: BLACK sibling node with left RED child node, ANY parent
					*    (P)               (P)                (P)
					*    / \               / \               /  \
					*   N   S      ->     N   sl     ->     N    SL
					*      / \                 \                  \
					*     sl (SR)               S                  s
					*                            \                  \
					*                            (SR)               (SR)
					*/
					rbtree_rotate(root, s, sl);
					s->color = RBT_RED;
					sl->color = RBT_BLACK;
					s = p->right;
					sl = s->left;
					sr = s->right;
				}
			}

			/* case 4: BLACK sibling node, left rotate P
			 *    (P)                S              (S)
			 *    / \               / \             / \
			 *   N   S      ->    (P)  sr    ->    P   SR
			 *      / \           / \             / \
			 *     SL  sr        N  SL           N  SL
			*/
			rbtree_rotate(root, p, s);
			s->color = p->color;
			p->color = RBT_BLACK;
			sr->color = RBT_BLACK;
			break;
		}
		else
		{
			s = p->left;

			if (s->color == RBT_RED)
			{
				// case 1
				rbtree_rotate(root, p, s);
				s->color = RBT_BLACK;
				p->color = RBT_RED;
				s = p->left;
			}

			assert(s->color == RBT_BLACK);
			sl = s->left;
			sr = s->right;
			if (!sl || sl->color == RBT_BLACK)
			{
				if (!sr || sr->color == RBT_BLACK)
				{
					// case 2
					s->color = RBT_RED;
					if (p->color == RBT_RED || !p->parent /*root*/)
					{
						p->color = RBT_BLACK;
						break;
					}
					else
					{
						node = p;
						continue;
					}
				}

				if (sr->color == RBT_RED)
				{
					// case 3
					rbtree_rotate(root, s, sr);
					s->color = RBT_RED;
					sr->color = RBT_BLACK;
					s = p->left;
					sl = s->left;
					sr = s->right;
				}
			}

			// case 4
			rbtree_rotate(root, p, s);
			s->color = p->color;
			p->color = RBT_BLACK;
			sl->color = RBT_BLACK;
			break;
		}
	}
}

void rbtree_insert(struct rbtree_root_t* root, struct rbtree_node_t* parent, struct rbtree_node_t** link, struct rbtree_node_t* node)
{
	// rb-tree link node
	node->left = node->right = RBT_SENTINEL;
	node->parent = parent;
	if(link) *link = node;

	// set color
	if (!root->node)
	{
		root->node = node; // root
		node->color = RBT_BLACK;
	}
	else
	{
		node->color = RBT_RED;
	}

	rbtree_insert_color(root, node);
}

static void rbtree_swap(struct rbtree_root_t* root, struct rbtree_node_t* node, struct rbtree_node_t* child)
{
	unsigned char color;
	struct rbtree_node_t* p; // parent
	struct rbtree_node_t* l; // left child
	struct rbtree_node_t* r; // right child

	assert(node && child);
	p = child->parent;
	l = child->left;
	r = child->right;

	if (node->left)
		node->left->parent = child;
	child->left = node->left == child ? node : node->left;
	if (node->right)
		node->right->parent = child;
	child->right = node->right == child ? node : node->right;
	if (node->parent)
	{
		if (node == node->parent->left)
			node->parent->left = child;
		else
			node->parent->right = child;
	}
	else
	{
		root->node = child;
	}
	child->parent = node->parent;

	if (p && p != node)
	{
		if (p->left == child)
			p->left = node;
		else
			p->right = node;
	}
	node->parent = p == node ? child : p;
	if (l) l->parent = node;
	node->left = l;
	if (r) r->parent = node;
	node->right = r;

	color = child->color;
	child->color = node->color;
	node->color = color;
}

static void rbtree_unlink(struct rbtree_root_t* root, struct rbtree_node_t* node)
{
	assert(!node->left && !node->right); // leaf node only

	if (node->parent)
	{
		if (node->parent->left == node)
			node->parent->left = 0;
		else
			node->parent->right = 0;
	}
	else
	{
		// root node
		root->node = 0;
	}
}

void rbtree_delete(struct rbtree_root_t* root, struct rbtree_node_t* node)
{
	struct rbtree_node_t* c; // child

	if (node->left && node->right)
	{
		c = node->right;
		while (c->left)
			c = c->left;

		// replace with successor child
		rbtree_swap(root, node, c);
	}
	
	assert(!node->left || !node->right);
	if (!node->left && !node->right)
	{
		if (RBT_RED == node->color)
		{
			rbtree_unlink(root, node);
		}
		else
		{
			rbtree_delete_color(root, node);
			rbtree_unlink(root, node);
		}
	}
	else if (!node->right)
	{
		assert(node->color == RBT_BLACK);
		assert(node->left && !node->right);
		assert(node->left->color == RBT_RED);
		rbtree_swap(root, node, node->left);
		rbtree_unlink(root, node);
	}
	else
	{
		assert(node->color == RBT_BLACK);
		assert(!node->left && node->right);
		assert(node->right->color == RBT_RED);
		rbtree_swap(root, node, node->right);
		rbtree_unlink(root, node);
	}
}

const struct rbtree_node_t* rbtree_first(const struct rbtree_root_t* root)
{
	struct rbtree_node_t* node;
	node = root->node;
	if (!node)
		return RBT_SENTINEL;

	while (node->left)
		node = node->left;
	return node;
}

const struct rbtree_node_t* rbtree_last(const struct rbtree_root_t* root)
{
	struct rbtree_node_t* node;
	node = root->node;
	if (!node)
		return RBT_SENTINEL;

	while (node->right)
		node = node->right;
	return node;
}

const struct rbtree_node_t* rbtree_prev(const struct rbtree_node_t* node)
{
	if (node->left)
	{
		node = node->left;
		while (node->right)
			node = node->right;
		return node;
	}
	else
	{
		while (node->parent && node == node->parent->left)
			node = node->parent;
		return node->parent;
	}
}

const struct rbtree_node_t* rbtree_next(const struct rbtree_node_t* node)
{
	if (node->right)
	{
		node = node->right;
		while (node->left)
			node = node->left;
		return node;
	}
	else
	{
		while (node->parent && node == node->parent->right)
			node = node->parent;
		return node->parent;
	}
}
