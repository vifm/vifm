#include <stic.h>

#include "../../src/utils/int_stack.h"

TEST(can_pop_from_empty_int_stack)
{
	int_stack_t stack = {};
	assert_true(int_stack_is_empty(&stack));
	int_stack_pop_seq(&stack, /*seq_quard=*/1);
}
