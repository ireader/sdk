#ifndef _rbtree_h_
#define _rbtree_h_

#if defined(__cplusplus)
extern "C" {
#endif

//#pragma pack(push, sizeof(long))
struct rbtree_node_t
{
	struct rbtree_node_t* left;
	struct rbtree_node_t* right;
	struct rbtree_node_t* parent;
	unsigned char color;
};
//#pragma pack(pop)

struct rbtree_root_t
{
	struct rbtree_node_t* node;
};

#define rbtree_entry(ptr, type, member) \
	(type*)((char*)(ptr) - (ptrdiff_t)&(((type*)0)->member))

/// re-banlance rb-tree(rbtree_link node before)
/// @param[in] root rbtree root node
/// @param[in] parent parent node
/// @param[in] link parent left or right child node address
/// @param[in] node insert node(new node)
void rbtree_insert(struct rbtree_root_t* root, struct rbtree_node_t* parent, struct rbtree_node_t** link, struct rbtree_node_t* node);

/// re-banlance rb-tree(rbtree_link node before)
/// @param[in] root rbtree root node
/// @param[in] node rbtree new node
void rbtree_delete(struct rbtree_root_t* root, struct rbtree_node_t* node);

const struct rbtree_node_t* rbtree_first(const struct rbtree_root_t* root);
const struct rbtree_node_t* rbtree_last(const struct rbtree_root_t* root);
const struct rbtree_node_t* rbtree_prev(const struct rbtree_node_t* node);
const struct rbtree_node_t* rbtree_next(const struct rbtree_node_t* node);

#if defined(__cplusplus)
}
#endif
#endif /* !_rbtree_h_ */
