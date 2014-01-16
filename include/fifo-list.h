#ifndef _fifo_list_h_
#define _fifo_list_h_

// lock-free FIFO list
// REF:
// 1. http://en.wikipedia.org/wiki/ABA_problem
// 2. http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.53.8674&rep=rep1&type=pdf

#include "sys/sync.h"
#include <assert.h>

struct fifo_node
{
	struct fifo_node *next;
};

struct fifo_list
{
	struct fifo_node head;
	struct fifo_node *tail;
};

inline int fifo_list_init(struct fifo_list *list)
{
	list->head.next = NULL;
	list->tail = &list->head;
}

inline void fifo_list_push(struct fifo_list *list, struct fifo_node* node)
{
	volatile struct fifo_node *tail;

	node->next = NULL;

	do
	{
		// TODO: fixed ABA problem
		tail = list->tail;
	} while(!atomic_cas((long*)&tail->next, (long)NULL, (long)node));

	assert(list->tail == tail);
	atomic_cas((long*)&list->tail, (long)tail, (long)node);
}

// Warning: only one thread can do pop action
inline struct fifo_node* fifo_list_pop(struct fifo_list *list)
{
	struct fifo_node *node;

	do
	{
		// TODO: fixed node be freed before access node->next
		node = list->head.next;
		if(!node)
			return NULL;

		// For simplicity, suppose that we can ensure that this dereference is safe
		// (i.e., that no other thread has popped the stack in the meantime).
	} while(!atomic_cas((long*)&list->head.next, (long)node, (long)node->next));

	return node;
}

inline struct fifo_node* fifo_list_head(struct fifo_list *list)
{
	return list->head.next;
}

inline int fifo_list_empty(struct fifo_list *list)
{
	return atomic_cas((long*)&list->head.next, (long)NULL, (long)NULL) ? 1 : 0;
}

inline int fifo_list_clear(struct fifo_list *list)
{
	volatile struct fifo_node *node;

	do 
	{
		node = list->head.next;
	} while (!atomic_cas((long*)&list->head.next, (long)node, (long)NULL));

	return 0;
}

#endif /* !_fifo_list_h_ */
