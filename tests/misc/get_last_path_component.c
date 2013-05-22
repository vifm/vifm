#include "seatest.h"

#include "../../src/utils/path.h"

static void
test_empty_path_ok(void)
{
	const char *const path = "";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path, last);
	assert_true(path == last);
}

static void
test_no_slashes_ok(void)
{
	const char *const path = "path_without_slashes";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path, last);
	assert_true(path == last);
}

static void
test_path_does_not_end_with_slash_ok(void)
{
	const char *const path = "/no/slashhere";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path + 4, last);
	assert_true(path + 4 == last);
}

static void
test_path_ends_with_slash_ok(void)
{
	const char *const path = "/slash/here/";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path + 7, last);
	assert_true(path + 7 == last);
}

static void
test_path_ends_with_multiple_slashes_ok(void)
{
	const char *const path = "/slash/here/////";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path + 7, last);
	assert_true(path + 7 == last);
}

static void
test_path_has_separators_of_multiple_slashes_ok(void)
{
	const char *const path = "/slashes///here";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path + 11, last);
	assert_true(path + 11 == last);
}

static void
test_one_element_ok(void)
{
	const char *const path = "/slashes";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path + 1, last);
	assert_true(path + 1 == last);
}

static void
test_root_ok(void)
{
	const char *const path = "/";
	const char *const last = get_last_path_component(path);
	assert_string_equal(path, last);
	assert_true(path == last);
}

void
get_last_path_component_tests(void)
{
	test_fixture_start();

	run_test(test_empty_path_ok);
	run_test(test_no_slashes_ok);
	run_test(test_path_does_not_end_with_slash_ok);
	run_test(test_path_ends_with_slash_ok);
	run_test(test_path_ends_with_multiple_slashes_ok);
	run_test(test_path_has_separators_of_multiple_slashes_ok);
	run_test(test_one_element_ok);
	run_test(test_root_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
