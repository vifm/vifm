#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

static var_t dummy(const call_info_t *call_info);

SETUP_ONCE()
{
	static const function_t function_a = { "a", 1, &dummy };
	static const function_t function_b = { "b", 2, &dummy };
	static const function_t function_c = { "c", 0, &dummy };

	assert_int_equal(0, function_register(&function_a));
	assert_int_equal(0, function_register(&function_b));
	assert_int_equal(0, function_register(&function_c));
}

static var_t
dummy(const call_info_t *call_info)
{
	static const var_val_t var_val = { .string = "" };
	return var_new(VTYPE_STRING, var_val);
}

TEST(wrong_number_of_arguments_fail)
{
	ASSERT_FAIL("a()", PE_INVALID_EXPRESSION);
	assert_string_equal("a()", get_last_position());
}

TEST(wrong_arg_fail)
{
	ASSERT_FAIL("a(a)", PE_INVALID_SUBEXPRESSION);
	assert_string_equal("a)", get_last_position());
}

TEST(two_args_ok)
{
	ASSERT_OK("b('a','b')", "");
}

TEST(space_before_first_arg_ok)
{
	ASSERT_OK("b( 'a','b')", "");
}

TEST(space_after_last_arg_ok)
{
	ASSERT_OK("b('a','b' )", "");
}

TEST(space_before_comma_ok)
{
	ASSERT_OK("b('a' ,'b')", "");
}

TEST(space_after_comma_ok)
{
	ASSERT_OK("b('a', 'b')", "");
}

TEST(concatenation_as_arg_ok)
{
	ASSERT_OK("a('a'.'b')", "");
}

TEST(no_args_ok)
{
	ASSERT_OK("c()", "");
}

TEST(no_function_name_fail)
{
	ASSERT_FAIL("()", PE_INVALID_EXPRESSION);
}

TEST(chars_after_function_call_fail)
{
	ASSERT_FAIL("a()a", PE_INVALID_EXPRESSION);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
