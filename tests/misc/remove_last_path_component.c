#include "seatest.h"

#include <limits.h> /* PATH_MAX */

#include "../../src/utils/path.h"

static void
test_empty_path(void)
{
	char path[PATH_MAX] = "";
	remove_last_path_component(path);
	assert_string_equal("", path);
}

static void
test_root_path(void)
{
	char path[PATH_MAX] = "/";
	remove_last_path_component(path);
	assert_string_equal("", path);
}

static void
test_path_does_not_end_with_slash(void)
{
	char path[PATH_MAX] = "/a/b/c";
	remove_last_path_component(path);
	assert_string_equal("/a/b", path);
}

static void
test_path_ends_with_slash(void)
{
	char path[PATH_MAX] = "/a/b/c/";
	remove_last_path_component(path);
	assert_string_equal("/a/b", path);
}

static void
test_path_ends_with_multiple_slashes(void)
{
	char path[PATH_MAX] = "/a/b/c///";
	remove_last_path_component(path);
	assert_string_equal("/a/b", path);
}

void
remove_last_path_component_tests(void)
{
	test_fixture_start();

	run_test(test_empty_path);
	run_test(test_root_path);
	run_test(test_path_does_not_end_with_slash);
	run_test(test_path_ends_with_slash);
	run_test(test_path_ends_with_multiple_slashes);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
