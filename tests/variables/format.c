#include <stdlib.h>

#include "seatest.h"

#include "../../src/engine/variables.h"

static void
test_full_tree_args_ok(void)
{
	assert_true(let_variable("$VAR = 'VAL'") == 0);
	assert_true(let_variable("$VAR .= 'VAL'") == 0);
}

static void
test_full_two_args_ok(void)
{
	assert_true(let_variable("$VAR ='VAL'") == 0);
	assert_true(let_variable("$VAR .='VAL'") == 0);
	assert_true(let_variable("$VAR= 'VAL'") == 0);
	assert_true(let_variable("$VAR.= 'VAL'") == 0);
}

static void
test_full_one_arg_ok(void)
{
	assert_true(let_variable("$VAR='VAL'") == 0);
	assert_true(let_variable("$VAR.='VAL'") == 0);
}

static void
test_no_quotes_fail(void)
{
	assert_false(let_variable("$VAR=VAL") == 0);
	assert_false(let_variable("$VAR.=VAL") == 0);
}

static void
test_single_quotes_ok(void)
{
	assert_true(let_variable("$VAR='VAL'") == 0);
	assert_true(let_variable("$VAR.='VAL'") == 0);
}

static void
test_double_quotes_ok(void)
{
	assert_true(let_variable("$VAR=\"VAL\"") == 0);
	assert_true(let_variable("$VAR.=\"VAL\"") == 0);
}

static void
test_trailing_spaces_ok(void)
{
	assert_true(let_variable("$VAR = \"VAL\" ") == 0);
	assert_true(let_variable("$VAR .= \"VAL\" ") == 0);
}

static void
test_too_many_arguments_fail(void)
{
	assert_false(let_variable("$VAR = \"VAL\" bbb") == 0);
	assert_false(let_variable("$VAR .= \"VAL\" $aaa") == 0);
}

static void
test_incomplete_two_args_fail(void)
{
	assert_false(let_variable("$VAR =") == 0);
	assert_false(let_variable("$VAR .=") == 0);
	assert_false(let_variable("= VAL") == 0);
	assert_false(let_variable(".= VAL") == 0);
}

static void
test_incomplete_one_arg_fail(void)
{
	assert_false(let_variable("$VAR") == 0);
	assert_false(let_variable("=") == 0);
	assert_false(let_variable(".=") == 0);
	assert_false(let_variable("VAL") == 0);
}

static void
test_no_dollar_sign_fail(void)
{
	assert_false(let_variable("VAR='VAL'") == 0);
	assert_false(let_variable("VAR.='VAL'") == 0);
}

static void
test_env_variable_empty_name_fail(void)
{
	assert_false(let_variable("$='VAL'") == 0);
	assert_false(let_variable("$.='VAL'") == 0);
}

static void
test_spaces_in_single_quotes_ok(void)
{
	assert_true(let_variable("$VAR='a b c'") == 0);
	assert_true(let_variable("$VAR.='a b c'") == 0);
}

static void
test_spaces_in_double_quotes_ok(void)
{
	assert_true(let_variable("$VAR=\"a b c\"") == 0);
	assert_true(let_variable("$VAR.=\"a b c\"") == 0);
}

static void
test_unlet_with_dollar_sign_ok(void)
{
	assert_true(unlet_variables("$VAR_A") == 0);
}

static void
test_unlet_no_name_fail(void)
{
	assert_true(unlet_variables("$") != 0);
}

static void
test_unlet_nonexistent_envvar_fail(void)
{
	assert_true(getenv("VAR") == NULL);
	assert_true(unlet_variables("$VAR") != 0);
}

static void
test_unlet_without_dollar_sign_fail(void)
{
	assert_true(unlet_variables("VAR_A") != 0);
}

static void
test_let_alnum_and_underscore_ok(void)
{
	assert_true(let_variable("$1_aZzA_0 = 'VAL'") == 0);
}

static void
test_let_wrong_symbols_fail(void)
{
	assert_true(let_variable("$.|a = 'VAL'") != 0);
}

void
format_tests(void)
{
	test_fixture_start();

	run_test(test_full_tree_args_ok);
	run_test(test_full_two_args_ok);
	run_test(test_full_one_arg_ok);

	run_test(test_no_quotes_fail);
	run_test(test_single_quotes_ok);
	run_test(test_double_quotes_ok);

	run_test(test_trailing_spaces_ok);
	run_test(test_too_many_arguments_fail);
	run_test(test_incomplete_two_args_fail);
	run_test(test_incomplete_one_arg_fail);

	run_test(test_no_dollar_sign_fail);
	run_test(test_env_variable_empty_name_fail);

	run_test(test_spaces_in_single_quotes_ok);
	run_test(test_spaces_in_double_quotes_ok);

	run_test(test_unlet_with_dollar_sign_ok);
	run_test(test_unlet_no_name_fail);
	run_test(test_unlet_nonexistent_envvar_fail);
	run_test(test_unlet_without_dollar_sign_fail);

	run_test(test_let_alnum_and_underscore_ok);
	run_test(test_let_wrong_symbols_fail);

	test_fixture_end();
}


/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
