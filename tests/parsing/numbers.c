#include "seatest.h"

#include <stdlib.h> /* free() */

#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

static void
setup(void)
{
	init_parser(NULL);
}

static void
test_negative_number_ok(void)
{
	ASSERT_OK("-1", "-1");
}

static void
test_multiple_negations_ok(void)
{
	ASSERT_OK("--1", "1");
	ASSERT_OK("---1", "-1");
	ASSERT_OK("----1", "1");
	ASSERT_OK("-----1", "-1");
}

static void
test_spaces_betwen_signs_ok(void)
{
	ASSERT_OK("--- 1", "-1");
	ASSERT_OK("- - + 1", "1");
	ASSERT_OK(" ++ + 1", "1");
}

static void
test_zero_ok(void)
{
	ASSERT_OK("0", "0");
	ASSERT_OK("-0", "0");
	ASSERT_OK("+0", "0");
}

static void
test_multiple_zeroes_ok(void)
{
	ASSERT_OK("00000", "0");
	ASSERT_OK("-00000", "0");
	ASSERT_OK("+00000", "0");
}

static void
test_positive_number_ok(void)
{
	ASSERT_OK("12345", "12345");
	ASSERT_OK("+12", "12");
	ASSERT_OK("++12", "12");
	ASSERT_OK("+++12", "12");
}

static void
test_leading_zeroes_ok(void)
{
	ASSERT_OK("0123456", "123456");
}

static void
test_single_signs_fail(void)
{
	ASSERT_FAIL("-", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("--", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("+", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("++", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("- +", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("+ -", PE_INVALID_EXPRESSION);
}

static void
test_string_is_converted_for_signs(void)
{
	ASSERT_OK("-'a'", "0");
	ASSERT_OK("+'a'", "0");
	ASSERT_OK("-'1'", "-1");
	ASSERT_OK("-'-1'", "1");
	ASSERT_OK("-'--1'", "0");
	ASSERT_OK("+'10'", "10");
}

void
numbers_tests(void)
{
	test_fixture_start();

	fixture_setup(&setup);

	run_test(test_negative_number_ok);
	run_test(test_multiple_negations_ok);
	run_test(test_spaces_betwen_signs_ok);
	run_test(test_zero_ok);
	run_test(test_multiple_zeroes_ok);
	run_test(test_positive_number_ok);
	run_test(test_leading_zeroes_ok);
	run_test(test_single_signs_fail);
	run_test(test_string_is_converted_for_signs);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
