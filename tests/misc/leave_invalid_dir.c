#include "seatest.h"

#include "../../src/filelist.h"

#ifndef _WIN32
#define ROOT "/"
#else
#define ROOT "C:/"
#endif

static void
test_empty_path(void)
{
	char path[PATH_MAX] = "";
	leave_invalid_dir(NULL, path);
	assert_string_equal(ROOT, path);
}

static void
test_wrong_path_without_trailing_slash(void)
{
	char path[PATH_MAX] = "/aaaaaaaaaaa/bbbbbbbbbbb/cccccccccc";
	leave_invalid_dir(NULL, path);
	assert_string_equal(ROOT, path);
}

static void
test_wrong_path_with_trailing_slash(void)
{
	char path[PATH_MAX] = "/aaaaaaaaaaa/bbbbbbbbbbb/cccccccccc/";
	leave_invalid_dir(NULL, path);
	assert_string_equal(ROOT, path);
}

void
leave_invalid_dir_tests(void)
{
	test_fixture_start();

	run_test(test_empty_path);
	run_test(test_wrong_path_without_trailing_slash);
	run_test(test_wrong_path_with_trailing_slash);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
