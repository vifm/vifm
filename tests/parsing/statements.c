#include "seatest.h"

#include <stdlib.h> /* free() */

#include "../../src/engine/functions.h"
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
}

static void
test_eq_compare_false(void)
{
	ASSERT_OK("'a'=='b'", "0");
}

static void
test_ne_compare_true(void)
{
	ASSERT_OK("'a'!='b'", "1");
}

static void
test_ne_compare_false(void)
{
	ASSERT_OK("'a'!='a'", "0");
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

void
statements_tests(void)
{
	test_fixture_start();

	fixture_setup(&setup);

	run_test(test_eq_compare_true);
	run_test(test_eq_compare_false);
	run_test(test_ne_compare_true);
	run_test(test_ne_compare_false);
	run_test(test_leading_spaces_ok);
	run_test(test_trailing_spaces_ok);
	run_test(test_spaces_before_op_ok);
	run_test(test_spaces_after_op_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
