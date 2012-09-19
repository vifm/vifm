#include "seatest.h"

#include <stdlib.h> /* free() */

#include "../../src/commands.h"

static void
test_one_arg(void)
{
	const char *args = "'a'";
	const char *stop_ptr;
	char *result;

	result = eval_echo(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("a", result);
	free(result);
}

static void
test_two_space_separated_args(void)
{
	const char *args = "'a'     'b'";
	const char *stop_ptr;
	char *result;

	result = eval_echo(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("a b", result);
	free(result);
}

static void
test_two_dot_separated_args(void)
{
	const char *args = "'a'  .  'b'";
	const char *stop_ptr;
	char *result;

	result = eval_echo(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("ab", result);
	free(result);
}

static void
test_double_single_quote(void)
{
	const char *args = "'a''b'";
	const char *stop_ptr;
	char *result;

	result = eval_echo(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("a'b", result);
	free(result);
}

static void
test_wrong_expression_position(void)
{
	const char *args = "'a' , some text";
	const char *stop_ptr;
	char *result;

	result = eval_echo(args, &stop_ptr);
	assert_true(result == NULL);
	assert_true(args + 4 == stop_ptr);
	free(result);
}

static void
test_empty_parens_fail(void)
{
	const char *args = "()";
	const char *stop_ptr;
	char *result;

	result = eval_echo(args, &stop_ptr);
	assert_true(result == NULL);
	free(result);
}

static void
test_chars_after_function_call_fail(void)
{
	const char *args = "a()a";
	const char *stop_ptr;
	char *result;

	result = eval_echo(args, &stop_ptr);
	assert_true(result == NULL);
	free(result);
}

void
echo_tests(void)
{
	test_fixture_start();

	run_test(test_one_arg);
	run_test(test_two_space_separated_args);
	run_test(test_two_dot_separated_args);
	run_test(test_double_single_quote);
	run_test(test_wrong_expression_position);
	run_test(test_empty_parens_fail);
	run_test(test_chars_after_function_call_fail);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
