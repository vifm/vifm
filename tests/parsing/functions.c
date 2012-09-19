#include <string.h> /* strdup() */

#include "seatest.h"

#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"

static var_t dummy(const call_info_t *call_info)
{
	return strdup("");
}

static void
setup(void)
{
	init_parser(NULL);
}

static void
test_wrong_arg_fail(void)
{
	const char *input = "a(a)";
	assert_true(parse(input) == NULL);
	assert_int_equal(PE_INVALID_SUBEXPRESSION, get_parsing_error());
	assert_true(input + 2 == get_last_position());
}

static void
test_two_args_ok(void)
{
	const char *input = "b('a','b')";
	const char *eval_res = parse(input);
	assert_true(eval_res != NULL);
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_space_before_first_arg_ok(void)
{
	const char *input = "b( 'a','b')";
	const char *eval_res = parse(input);
	assert_true(eval_res != NULL);
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_space_after_last_arg_ok(void)
{
	const char *input = "b('a','b' )";
	const char *eval_res = parse(input);
	assert_true(eval_res != NULL);
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_space_before_comma_ok(void)
{
	const char *input = "b('a' ,'b')";
	const char *eval_res = parse(input);
	assert_true(eval_res != NULL);
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_space_after_comma_ok(void)
{
	const char *input = "b('a', 'b')";
	const char *eval_res = parse(input);
	assert_true(eval_res != NULL);
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_concatenation_as_arg_ok(void)
{
	const char *input = "a('a'.'b')";
	const char *eval_res = parse(input);
	assert_true(eval_res != NULL);
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_no_args_ok(void)
{
	const char *input = "c()";
	const char *eval_res = parse(input);
	assert_true(eval_res != NULL);
	assert_int_equal(PE_NO_ERROR, get_parsing_error());
}

static void
test_no_function_name_fail(void)
{
	const char *input = "()";
	const char *eval_res = parse(input);
	assert_true(eval_res == NULL);
	assert_int_equal(PE_INVALID_EXPRESSION, get_parsing_error());
}

static void
test_chars_after_function_call_fail(void)
{
	const char *input = "a()a";
	const char *eval_res = parse(input);
	assert_true(eval_res == NULL);
	assert_int_equal(PE_INVALID_EXPRESSION, get_parsing_error());
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
