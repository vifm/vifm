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
test_logical_not_of_integers(void)
{
	ASSERT_INT_OK("!1", 0);
	ASSERT_INT_OK("!0", 1);
	ASSERT_INT_OK("!-10", 0);
}

static void
test_logical_not_of_strings(void)
{
	ASSERT_INT_OK("!'abc'", 0);
	ASSERT_INT_OK("!''", 1);
}

static void
test_multiple_logical_not(void)
{
	ASSERT_INT_OK("!!10", 1);
	ASSERT_INT_OK("!!!10", 0);
}

static void
test_extra_spaces_in_logical_not(void)
{
	ASSERT_INT_OK("! 18", 0);
	ASSERT_INT_OK("! \"\"", 1);
	ASSERT_INT_OK("! !! \"\"", 1);
}

static void
test_invalid_logical_not_without_argument(void)
{
	ASSERT_FAIL("!", PE_INVALID_EXPRESSION);
}

void
unary_tests(void)
{
	test_fixture_start();

	fixture_setup(&setup);

	run_test(test_logical_not_of_integers);
	run_test(test_logical_not_of_strings);
	run_test(test_multiple_logical_not);
	run_test(test_extra_spaces_in_logical_not);
	run_test(test_invalid_logical_not_without_argument);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
