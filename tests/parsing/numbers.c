#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* memset() */

#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

TEST(negative_number_ok)
{
	ASSERT_OK("-1", "-1");
}

TEST(multiple_negations_ok)
{
	ASSERT_OK("--1", "1");
	ASSERT_OK("---1", "-1");
	ASSERT_OK("----1", "1");
	ASSERT_OK("-----1", "-1");
}

TEST(spaces_betwen_signs_ok)
{
	ASSERT_OK("--- 1", "-1");
	ASSERT_OK("- - + 1", "1");
	ASSERT_OK(" ++ + 1", "1");
}

TEST(zero_ok)
{
	ASSERT_OK("0", "0");
	ASSERT_OK("-0", "0");
	ASSERT_OK("+0", "0");
}

TEST(multiple_zeroes_ok)
{
	ASSERT_OK("00000", "0");
	ASSERT_OK("-00000", "0");
	ASSERT_OK("+00000", "0");
}

TEST(positive_number_ok)
{
	ASSERT_OK("12345", "12345");
	ASSERT_OK("+12", "12");
	ASSERT_OK("++12", "12");
	ASSERT_OK("+++12", "12");
}

TEST(leading_zeroes_ok)
{
	ASSERT_OK("0123456", "123456");
}

TEST(single_signs_fail)
{
	ASSERT_FAIL("-", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("--", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("+", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("++", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("- +", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("+ -", PE_INVALID_EXPRESSION);
}

TEST(string_is_converted_for_signs)
{
	ASSERT_OK("-'a'", "0");
	ASSERT_OK("+'a'", "0");
	ASSERT_OK("-'1'", "-1");
	ASSERT_OK("-'0'", "0");
	ASSERT_OK("-''", "0");
	ASSERT_OK("-'-1'", "1");
	ASSERT_OK("-'--1'", "0");
	ASSERT_OK("+'10'", "10");
	ASSERT_OK("+'-100'", "-100");
}

TEST(extremely_long_number)
{
	var_t res_var = var_false();

	char zeroes[8192];
	memset(zeroes, '0', sizeof(zeroes) - 1U);
	zeroes[sizeof(zeroes) - 1U] = '\0';

	assert_int_equal(PE_INTERNAL, parse(zeroes, 0, &res_var));
	var_free(res_var);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
