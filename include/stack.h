#ifndef _stack_h_
#define _stack_h_

// lock-free stack

#include "sys/atomic.h"

#define stack_entry(ptr, type, member) \
	(type*)((char*)ptr-(unsigned long)(&((type*)0)->member))

struct stack_node
{
	struct stack_node *volatile next;
};

struct stack
{
	struct stack_node *volatile top;
};

static inline int stack_init(struct stack *stack)
{
	stack->top = NULL;
	return 0;
}

static inline void stack_push(struct stack *stack, struct stack_node* node)
{
	struct stack_node *volatile top;

	do
	{
		top = stack->top;
		node->next = top;
	} while(!atomic_cas_ptr((void* volatile*)&stack->top, top, node));
}

// Warning: only one thread can do pop action
static inline void stack_pop(struct stack *stack)
{
	struct stack_node *volatile top;

	do
	{
		// TODO: fixed node be freed before access node->next
		top = stack->top;
		if(!top)
			return;

		// For simplicity, suppose that we can ensure that this dereference is safe
		// (i.e., that no other thread has popped the stack in the meantime).
	} while(!atomic_cas_ptr((void* volatile*)&stack->top, top, top->next));
}

static inline struct stack_node* stack_top(struct stack *stack)
{
	return stack->top;
}

static inline int stack_empty(struct stack *stack)
{
	return atomic_cas_ptr((void* volatile*)&stack->top, NULL, NULL) ? 1 : 0;
}

static inline int stack_clear(struct stack *stack)
{
	struct stack_node *volatile top;

	do 
	{
		top = stack->top;
	} while (!atomic_cas_ptr((void* volatile*)&stack->top, top, NULL));

	return 0;
}

#endif /* !_stack_h_ */
