#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

static var_t dummy(const call_info_t *call_info);

SETUP_ONCE()
{
	static const function_t function_a = { "a", "adescr", {1,1}, &dummy };
	static const function_t function_b = { "b", "bdescr", {2,2}, &dummy };
	static const function_t function_c = { "c", "cdescr", {0,0}, &dummy };
	static const function_t function_d = { "d", "ddescr", {1,3}, &dummy };

	assert_success(function_register(&function_a));
	assert_success(function_register(&function_b));
	assert_success(function_register(&function_c));
	assert_success(function_register(&function_d));
}

TEARDOWN_ONCE()
{
	function_reset_all();
}

static var_t
dummy(const call_info_t *call_info)
{
	return var_from_str("");
}

TEST(function_with_wrong_signature_is_not_added)
{
	static const function_t function = { "d", "ddescr", {3,2}, &dummy };
	assert_failure(function_register(&function));
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

TEST(or_as_arg_ok)
{
	ASSERT_OK("a('a'||'b')", "");
}

TEST(and_as_arg_ok)
{
	ASSERT_OK("a('a'&&'b')", "");
}

TEST(no_args_ok)
{
	ASSERT_OK("c()", "");
}

TEST(range_of_args)
{
	ASSERT_FAIL("d()", PE_INVALID_EXPRESSION);

	ASSERT_OK("d(1)", "");
	ASSERT_OK("d(1, 2)", "");
	ASSERT_OK("d(1, 2, 3)", "");

	ASSERT_FAIL("d(1, 2, 3, 4)", PE_INVALID_EXPRESSION);
}

TEST(no_function_name_fail)
{
	ASSERT_FAIL("()", PE_INVALID_EXPRESSION);
}

TEST(chars_after_function_call_fail)
{
	ASSERT_FAIL("a()a", PE_INVALID_EXPRESSION);
}

TEST(very_long_function_name)
{
	const char *const input = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaafffffffffffffffffffffffff"
		"fffffffff";

	ASSERT_FAIL(input, PE_INVALID_EXPRESSION);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
