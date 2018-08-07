#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */

#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"
#include "../../src/utils/env.h"
#include "../../src/cmd_core.h"

static var_t echo_builtin(const call_info_t *call_info);

SETUP_ONCE()
{
	static const function_t echo_function = {
		"a", "descr", {1,1}, &echo_builtin
	};
	assert_success(function_register(&echo_function));

	init_parser(&env_get);
}

static var_t
echo_builtin(const call_info_t *call_info)
{
	return var_clone(call_info->argv[0]);
}

TEST(broken_syntax)
{
	const char *args = "a('foo '; bar";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_string_equal(NULL, result);
}

TEST(one_arg)
{
	const char *args = "'a'";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("a", result);
	free(result);
}

TEST(two_space_separated_args)
{
	const char *args = "'a'     'b'";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("a b", result);
	free(result);
}

TEST(two_dot_separated_args)
{
	const char *args = "'a'  .  'b'";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("ab", result);
	free(result);
}

TEST(double_single_quote)
{
	const char *args = "'a''b'";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("a'b", result);
	free(result);
}

TEST(wrong_expression_position)
{
	const char *args = "'a' , some text";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result == NULL);
	assert_true(args + 4 == stop_ptr);
	free(result);
}

TEST(empty_parens_fail)
{
	const char *args = "()";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result == NULL);
	free(result);
}

TEST(chars_after_function_call_fail)
{
	const char *args = "a()a";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result == NULL);
	free(result);
}

TEST(expression)
{
	const char *args = "'a'=='a'";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("1", result);
	free(result);
}

TEST(expression_and_literal)
{
	const char *args = "'a'=='a' 'b'";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("1 b", result);
	free(result);
}

TEST(call_function)
{
	const char *args = "a('hello')";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result != NULL);
	assert_string_equal("hello", result);
	free(result);
}

TEST(broken_comparison_operator)
{
	const char *args = "'a' < = 'b'";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_true(result == NULL);
	free(result);
}

TEST(and_operator)
{
	const char *args = "'a' && 'b' && 'c'";
	const char *stop_ptr;
	char *result;

	result = eval_arglist(args, &stop_ptr);
	assert_string_equal("0", result);
	free(result);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
