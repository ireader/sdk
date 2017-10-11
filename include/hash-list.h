#ifndef _hash_list_h_
#define _hash_list_h_

struct hash_node_t
{
	struct hash_node_t* next;
	struct hash_node_t** pnext; // &prev->next
};

struct hash_head_t
{
	struct hash_node_t *first;
};

static inline int hash_list_empty(struct hash_head_t* head)
{
	return !head->first;
}

static inline void hash_list_link(struct hash_head_t* head, struct hash_node_t* node)
{
	if (head->first)
		head->first->pnext = &node->next;
	node->next = head->first;
	node->pnext = &head->first;
	head->first = node;
}

static inline void hash_list_unlink(struct hash_node_t* node)
{
	*node->pnext = node->next;
	if (node->next)
		node->next->pnext = node->pnext;
}

#define hash_list_entry(ptr, type, member)	\
	((type*)( (char*)ptr - (intptr_t)&(((type*)0)->member) ))

#define hash_list_for_each(pos, head) \
	for(pos = (head)->first; pos; pos = pos->next)

#define hash_list_for_each_safe(pos, n, head) \
	for(pos = (head)->first; pos && (n = pos->next, 1); pos = n)

#endif /* !_hash_table_h_ */
