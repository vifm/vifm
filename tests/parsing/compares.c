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
test_eq_compare_true(void)
{
	ASSERT_OK("'a'=='a'", "1");
	ASSERT_OK("1==1", "1");
}

static void
test_eq_compare_false(void)
{
	ASSERT_OK("'a'=='b'", "0");
	ASSERT_OK("1==-1", "0");
}

static void
test_ne_compare_true(void)
{
	ASSERT_OK("'a'!='b'", "1");
	ASSERT_OK("-1!=1", "1");
}

static void
test_ne_compare_false(void)
{
	ASSERT_OK("'a'!='a'", "0");
	ASSERT_OK("0!=0", "0");
}

static void
test_lt_compare_true(void)
{
	ASSERT_OK("1<2", "1");
	ASSERT_OK("'a'<'b'", "1");
}

static void
test_lt_compare_false(void)
{
	ASSERT_OK("2<1", "0");
	ASSERT_OK("' '<'a'", "1");
}

static void
test_le_compare_true(void)
{
	ASSERT_OK("1<=1", "1");
	ASSERT_OK("-1<=1", "1");
	ASSERT_OK("'a'<='b'", "1");
	ASSERT_OK("'b'<='b'", "1");
}

static void
test_le_compare_false(void)
{
	ASSERT_OK("3<=2", "0");
	ASSERT_OK("'c'<='b'", "0");
}

static void
test_ge_compare_true(void)
{
	ASSERT_OK("1>=1", "1");
	ASSERT_OK("2>=1", "1");
	ASSERT_OK("'b'>='a'", "1");
	ASSERT_OK("'b'>='b'", "1");
}

static void
test_ge_compare_false(void)
{
	ASSERT_OK("0>=1", "0");
	ASSERT_OK("'b'>='c'", "0");
}

static void
test_gt_compare_true(void)
{
	ASSERT_OK("2>-1", "1");
	ASSERT_OK("'b'>'a'", "1");
}

static void
test_gt_compare_false(void)
{
	ASSERT_OK("1>1", "0");
	ASSERT_OK("'b'>'b'", "0");
}

static void
test_compares_prefer_numbers(void)
{
	ASSERT_OK("'2'>'b'", "0");
	ASSERT_OK("2>'b'", "1");
}

static void
test_compares_convert_string_to_numbers(void)
{
	ASSERT_OK("2>'b'", "1");
	ASSERT_OK("2>'9b'", "0");
}

static void
test_leading_spaces_ok(void)
{
	ASSERT_OK("     'a'!='a'", "0");
}

static void
test_trailing_spaces_ok(void)
{
	ASSERT_OK("'a'!='a'       ", "0");
}

static void
test_spaces_before_op_ok(void)
{
	ASSERT_OK("'a'      !='a'", "0");
}

static void
test_spaces_after_op_ok(void)
{
	ASSERT_OK("'a'!=         'a'", "0");
}

static void
test_spaces_in_op_fail(void)
{
	ASSERT_FAIL("'a' ! = 'a'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' = = 'a'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' > = 'a'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' < = 'a'", PE_INVALID_EXPRESSION);
}

static void
test_wrong_op_fail(void)
{
	ASSERT_FAIL("'a' =! 'a'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' => 'a'", PE_INVALID_EXPRESSION);
	ASSERT_FAIL("'a' =< 'a'", PE_INVALID_EXPRESSION);
}

void
compares_tests(void)
{
	test_fixture_start();

	fixture_setup(&setup);

	run_test(test_eq_compare_true);
	run_test(test_eq_compare_false);
	run_test(test_ne_compare_true);
	run_test(test_ne_compare_false);
	run_test(test_lt_compare_true);
	run_test(test_lt_compare_false);
	run_test(test_le_compare_true);
	run_test(test_le_compare_false);
	run_test(test_ge_compare_true);
	run_test(test_ge_compare_false);
	run_test(test_gt_compare_true);
	run_test(test_gt_compare_false);
	run_test(test_compares_prefer_numbers);
	run_test(test_compares_convert_string_to_numbers);
	run_test(test_leading_spaces_ok);
	run_test(test_trailing_spaces_ok);
	run_test(test_spaces_before_op_ok);
	run_test(test_spaces_after_op_ok);
	run_test(test_spaces_in_op_fail);
	run_test(test_wrong_op_fail);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
