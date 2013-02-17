#include "seatest.h"

#include "../../src/fileops.h"

static void
test_substitute_segfault_bug(void)
{
	/* see #SF3515922 */
	assert_string_equal("barfoobar", substitute_in_name("foobar", "^", "bar", 1));
}

static void
test_substitute_begin_global(void)
{
	assert_string_equal("01", substitute_in_name("001", "^0", "", 1));
}

static void
test_substitute_end_global(void)
{
	assert_string_equal("10", substitute_in_name("100", "0$", "", 1));
}

void
substitute_tests(void)
{
	test_fixture_start();

	run_test(test_substitute_segfault_bug);
	run_test(test_substitute_begin_global);
	run_test(test_substitute_end_global);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
