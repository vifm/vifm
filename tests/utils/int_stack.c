#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/utils/int_stack.h"

TEST(can_pop_from_empty_int_stack)
{
	int_stack_t stack = {};
	assert_true(int_stack_is_empty(&stack));
	int_stack_pop_seq(&stack, /*seq_quard=*/1);
}

TEST(can_clear_int_stack)
{
	int_stack_t stack = {};
	assert_success(int_stack_push(&stack, 10));
	assert_false(int_stack_is_empty(&stack));
	int_stack_clear(&stack);
	assert_true(int_stack_is_empty(&stack));
	free(stack.data);
}
