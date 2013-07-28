#include "seatest.h"

#include <stdlib.h> /* free() */

#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

static var_t
dummy(const call_info_t *call_info)
{
	static const var_val_t var_val = { .string = "" };
	return var_new(VTYPE_STRING, var_val);
}

static void
setup(void)
{
	init_parser(NULL);
}

static void
test_wrong_number_of_arguments_fail(void)
{
	ASSERT_FAIL("a()", PE_INVALID_EXPRESSION);
	assert_string_equal("a()", get_last_position());
}

static void
test_wrong_arg_fail(void)
{
	ASSERT_FAIL("a(a)", PE_INVALID_SUBEXPRESSION);
	assert_string_equal("a)", get_last_position());
}

static void
test_two_args_ok(void)
{
	ASSERT_OK("b('a','b')", "");
}

static void
test_space_before_first_arg_ok(void)
{
	ASSERT_OK("b( 'a','b')", "");
}

static void
test_space_after_last_arg_ok(void)
{
	ASSERT_OK("b('a','b' )", "");
}

static void
test_space_before_comma_ok(void)
{
	ASSERT_OK("b('a' ,'b')", "");
}

static void
test_space_after_comma_ok(void)
{
	ASSERT_OK("b('a', 'b')", "");
}

static void
test_concatenation_as_arg_ok(void)
{
	ASSERT_OK("a('a'.'b')", "");
}

static void
test_no_args_ok(void)
{
	ASSERT_OK("c()", "");
}

static void
test_no_function_name_fail(void)
{
	ASSERT_FAIL("()", PE_INVALID_EXPRESSION);
}

static void
test_chars_after_function_call_fail(void)
{
	ASSERT_FAIL("a()a", PE_INVALID_EXPRESSION);
}

void
functions_tests(void)
{
	static const function_t function_a = { "a", 1, &dummy };
	static const function_t function_b = { "b", 2, &dummy };
	static const function_t function_c = { "c", 0, &dummy };

	test_fixture_start();

	assert_int_equal(0, function_register(&function_a));
	assert_int_equal(0, function_register(&function_b));
	assert_int_equal(0, function_register(&function_c));

	fixture_setup(&setup);

	run_test(test_wrong_number_of_arguments_fail);
	run_test(test_wrong_arg_fail);
	run_test(test_two_args_ok);
	run_test(test_space_before_first_arg_ok);
	run_test(test_space_after_last_arg_ok);
	run_test(test_space_before_comma_ok);
	run_test(test_space_after_comma_ok);
	run_test(test_concatenation_as_arg_ok);
	run_test(test_no_args_ok);
	run_test(test_no_function_name_fail);
	run_test(test_chars_after_function_call_fail);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
