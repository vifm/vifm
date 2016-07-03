/* vifm
 * Copyright (C) 2012 xaizek.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "int_stack.h"

#include <assert.h> /* assert() */
#include <stddef.h> /* size_t */
#include <stdlib.h> /* realloc() */

static int ensure_available(int_stack_t *stack);

int
int_stack_is_empty(const int_stack_t *stack)
{
	return stack->top == 0;
}

int
int_stack_get_top(const int_stack_t *stack)
{
	assert(!int_stack_is_empty(stack));

	return stack->data[stack->top - 1];
}

int
int_stack_top_is(const int_stack_t *stack, int val)
{
	return !int_stack_is_empty(stack) && int_stack_get_top(stack) == val;
}

void
int_stack_set_top(const int_stack_t *stack, int val)
{
	assert(!int_stack_is_empty(stack));

	stack->data[stack->top - 1] = val;
}

int
int_stack_push(int_stack_t *stack, int val)
{
	if(ensure_available(stack) == 0)
	{
		stack->data[stack->top++] = val;
		return 0;
	}
	return 1;
}

/* Ensures that there is a place for one more element in the stack.  Returns
 * non-zero if not enough memory. */
static int
ensure_available(int_stack_t *stack)
{
	if(stack->top == stack->len)
	{
		int *const ptr = realloc(stack->data, sizeof(int)*(stack->len + 1));
		if(ptr != NULL)
		{
			stack->data = ptr;
			++stack->len;
		}
	}
	return (stack->top < stack->len) ? 0 : 1;
}

void
int_stack_pop(int_stack_t *stack)
{
	assert(!int_stack_is_empty(stack));

	--stack->top;
}

void
int_stack_pop_seq(int_stack_t *stack, int seq_guard)
{
	while(--stack->top > 0 && stack->data[stack->top] != seq_guard)
	{
		/* Do nothing. */
	}
}

void
int_stack_clear(int_stack_t *stack)
{
	stack->top = 0U;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
