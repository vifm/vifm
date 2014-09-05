#include "seatest.h"

#include <unistd.h> /* chdir() */

void invoked_tests(void);

static void
setup(void)
{
	assert_int_equal(0, chdir("test-data/sandbox"));
}

static void
teardown(void)
{
	assert_int_equal(0, chdir("../.."));
}

void
all_tests(void)
{
	invoked_tests();
}

int
main(void)
{
	suite_setup(setup);
	suite_teardown(teardown);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
