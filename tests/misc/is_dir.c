#include "seatest.h"

#include "../../src/utils/fs.h"

static void
test_file_is_not_recognized_as_dir(void)
{
	assert_false(is_dir("test-data/existing-files/a"));
}

static void
test_dir_is_not_recognized_as_dir(void)
{
	assert_true(is_dir("test-data/existing-files"));
}

static void
test_trailing_slash_is_ok(void)
{
	assert_true(is_dir("test-data/existing-files/"));
}

void
is_dir_tests(void)
{
	test_fixture_start();

	run_test(test_file_is_not_recognized_as_dir);
	run_test(test_dir_is_not_recognized_as_dir);
	run_test(test_trailing_slash_is_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
