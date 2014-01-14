#ifndef _stack_h_
#define _stack_h_

// lock-free stack

#include "sys/sync.h"

struct stack_node
{
	struct stack_node *next;
};

struct stack
{
	struct stack_node* top;
};

inline int stack_init(struct stack *stack)
{
	stack->top = NULL;
	return 0;
}

inline void stack_push(struct stack *stack, struct stack_node* node)
{
	struct stack_node *top;

	do
	{
		top = stack->top;
		node->next = top;
	} while(!atomic_cas((long*)&stack->top, (long)top, (long)node));
}

// Warning: only one thread can do pop action
inline struct stack_node* stack_pop(struct stack *stack)
{
	struct stack_node *top;

	do
	{
		// TODO: fixed node be freed before access node->next
		top = stack->top;
		if(!top)
			return NULL;

		// For simplicity, suppose that we can ensure that this dereference is safe
		// (i.e., that no other thread has popped the stack in the meantime).
	} while(!atomic_cas((long*)&stack->top, (long)top, (long)top->next));

	return top;
}

inline int stack_empty(struct stack *stack)
{
	return atomic_cas((long*)&stack->top, (long)NULL, (long)NULL) ? 1 : 0;
}

inline int stack_clear(struct stack *stack)
{
	volatile struct stack_node *top;

	do 
	{
		top = stack->top;
	} while (!atomic_cas((long*)&stack->top, (long)top, (long)NULL));

	return 0;
}

#endif /* !_stack_h_ */
