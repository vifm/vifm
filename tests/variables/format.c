#include <stic.h>

#include <stdlib.h>
#include <string.h> /* memset() */

#include "../../src/engine/variables.h"

TEST(full_tree_args_ok)
{
	assert_true(let_variables("$VAR = 'VAL'") == 0);
	assert_true(let_variables("$VAR .= 'VAL'") == 0);
}

TEST(full_two_args_ok)
{
	assert_true(let_variables("$VAR ='VAL'") == 0);
	assert_true(let_variables("$VAR .='VAL'") == 0);
	assert_true(let_variables("$VAR= 'VAL'") == 0);
	assert_true(let_variables("$VAR.= 'VAL'") == 0);
}

TEST(full_one_arg_ok)
{
	assert_true(let_variables("$VAR='VAL'") == 0);
	assert_true(let_variables("$VAR.='VAL'") == 0);
}

TEST(no_quotes_fail)
{
	assert_false(let_variables("$VAR=VAL") == 0);
	assert_false(let_variables("$VAR.=VAL") == 0);
}

TEST(single_quotes_ok)
{
	assert_true(let_variables("$VAR='VAL'") == 0);
	assert_true(let_variables("$VAR.='VAL'") == 0);
}

TEST(double_quotes_ok)
{
	assert_true(let_variables("$VAR=\"VAL\"") == 0);
	assert_true(let_variables("$VAR.=\"VAL\"") == 0);
}

TEST(trailing_spaces_ok)
{
	assert_true(let_variables("$VAR = \"VAL\" ") == 0);
	assert_true(let_variables("$VAR .= \"VAL\" ") == 0);
}

TEST(too_many_arguments_fail)
{
	assert_false(let_variables("$VAR = \"VAL\" bbb") == 0);
	assert_false(let_variables("$VAR .= \"VAL\" $aaa") == 0);
}

TEST(incomplete_two_args_fail)
{
	assert_false(let_variables("$VAR =") == 0);
	assert_false(let_variables("$VAR .=") == 0);
	assert_false(let_variables("= VAL") == 0);
	assert_false(let_variables(".= VAL") == 0);
}

TEST(incomplete_one_arg_fail)
{
	assert_false(let_variables("$VAR") == 0);
	assert_false(let_variables("=") == 0);
	assert_false(let_variables(".=") == 0);
	assert_false(let_variables("VAL") == 0);
}

TEST(no_dollar_sign_fail)
{
	assert_false(let_variables("VAR='VAL'") == 0);
	assert_false(let_variables("VAR.='VAL'") == 0);
}

TEST(env_variable_empty_name_fail)
{
	assert_false(let_variables("$='VAL'") == 0);
	assert_false(let_variables("$.='VAL'") == 0);
}

TEST(spaces_in_single_quotes_ok)
{
	assert_true(let_variables("$VAR='a b c'") == 0);
	assert_true(let_variables("$VAR.='a b c'") == 0);
}

TEST(spaces_in_double_quotes_ok)
{
	assert_true(let_variables("$VAR=\"a b c\"") == 0);
	assert_true(let_variables("$VAR.=\"a b c\"") == 0);
}

TEST(unlet_with_dollar_sign_ok)
{
	assert_true(unlet_variables("$VAR_A") == 0);
}

TEST(unlet_no_name_fail)
{
	assert_true(unlet_variables("$") != 0);
}

TEST(unlet_nonexistent_envvar_fail)
{
	assert_true(getenv("VAR") == NULL);
	assert_true(unlet_variables("$VAR") != 0);
}

TEST(unlet_without_dollar_sign_fail)
{
	assert_true(unlet_variables("VAR_A") != 0);
}

TEST(let_alnum_and_underscore_ok)
{
	assert_true(let_variables("$a1_aZzA_0 = 'VAL'") == 0);
}

TEST(let_wrong_symbols_fail)
{
	assert_true(let_variables("$.|a = 'VAL'") != 0);
}

TEST(too_long_input)
{
	char zeroes[8192];
	memset(zeroes, '0', sizeof(zeroes) - 1U);
	zeroes[sizeof(zeroes) - 1U] = '\0';

	assert_true(let_variables(zeroes) != 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
