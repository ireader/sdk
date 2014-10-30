#include "cstringext.h"
#include "stack.h"
#include <assert.h>

struct stack_value_t
{
	struct stack_node link;

	int value;
};

static void stack_base_test(void)
{
	struct stack stack;
	struct stack_value_t v1, v2, v3, v4, v5, v6;

	v1.value = 1;
	v2.value = 2;
	v3.value = 3;
	v4.value = 4;
	v5.value = 5;
	v6.value = 6;

	stack_init(&stack);
	stack_push(&stack, &v1.link);
	assert((stack_entry(stack_top(&stack), struct stack_value_t, link))->value == 1);
	stack_push(&stack, &v2.link);
	assert((stack_entry(stack_top(&stack), struct stack_value_t, link))->value == 2);
	stack_push(&stack, &v3.link);
	assert((stack_entry(stack_top(&stack), struct stack_value_t, link))->value == 3);
	stack_push(&stack, &v4.link);
	assert((stack_entry(stack_top(&stack), struct stack_value_t, link))->value == 4);
	stack_push(&stack, &v5.link);
	assert((stack_entry(stack_top(&stack), struct stack_value_t, link))->value == 5);
	stack_push(&stack, &v6.link);
	assert((stack_entry(stack_top(&stack), struct stack_value_t, link))->value == 6);

	stack_pop(&stack);
	assert((stack_entry(stack_top(&stack), struct stack_value_t, link))->value == 5);
	stack_pop(&stack);
	assert((stack_entry(stack_top(&stack), struct stack_value_t, link))->value == 4);
	stack_pop(&stack);
	assert((stack_entry(stack_top(&stack), struct stack_value_t, link))->value == 3);
	stack_pop(&stack);
	assert((stack_entry(stack_top(&stack), struct stack_value_t, link))->value == 2);

	assert(!stack_empty(&stack));
	stack_clear(&stack);
	assert(stack_empty(&stack));
}

void stack_test(void)
{
	stack_base_test();
}
