#include "seatest.h"

#include "../../src/utils/utils.h"

static void
test_yes(void)
{
	assert_true(path_starts_with("/home/trash", "/home/trash"));
	assert_true(path_starts_with("/home/trash/", "/home/trash"));
	assert_true(path_starts_with("/home/trash/", "/home/trash/"));
	assert_true(path_starts_with("/home/trash", "/home/trash/"));
}

static void
test_no(void)
{
	assert_false(path_starts_with("/home/tras", "/home/trash"));
	assert_false(path_starts_with("/home/trash_", "/home/trash"));
}

void
path_starts_with_tests(void)
{
	test_fixture_start();

	run_test(test_yes);
	run_test(test_no);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
