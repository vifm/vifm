#include "seatest.h"

#include <stdlib.h> /* free() */

#include "../../src/engine/functions.h"
#include "../../src/engine/var.h"
#include "../../src/commands.h"

static var_t
echo_builtin(const call_info_t *call_info)
{
	return var_clone(call_info->argv[0]);
}

static void
test_one_arg(void)
{
	const char *args = "'a'";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
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

	result = eval_arglist(args, &stop_ptr);
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

	result = eval_arglist(args, &stop_ptr);
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

	result = eval_arglist(args, &stop_ptr);
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

	result = eval_arglist(args, &stop_ptr);
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

	result = eval_arglist(args, &stop_ptr);
	assert_true(result == NULL);
	free(result);
}

static void
test_chars_after_function_call_fail(void)
{
	const char *args = "a()a";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result == NULL);
	free(result);
}

static void
test_statement(void)
{
	const char *args = "'a'=='a'";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("1", result);
	free(result);
}

static void
test_statement_and_not_statement(void)
{
	const char *args = "'a'=='a' 'b'";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("1 b", result);
	free(result);
}

static void
test_function_call(void)
{
	const char *args = "a('hello')";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("hello", result);
	free(result);
}

void
echo_tests(void)
{
	static const function_t echo_function = { "a", 1, &echo_builtin };

	test_fixture_start();

	assert_int_equal(0, function_register(&echo_function));

	run_test(test_one_arg);
	run_test(test_two_space_separated_args);
	run_test(test_two_dot_separated_args);
	run_test(test_double_single_quote);
	run_test(test_wrong_expression_position);
	run_test(test_empty_parens_fail);
	run_test(test_chars_after_function_call_fail);
	run_test(test_statement);
	run_test(test_statement_and_not_statement);
	run_test(test_function_call);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
