#include "seatest.h"

#include "../../src/fileops.h"
#include "../../src/macros.h"

static void
test_names_less_than_files(void)
{
	char *src[] = { "a", "b" };
	char *dst[] = { "a" };
	assert_false(is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));
}

static void
test_names_greater_that_files_fail(void)
{
	char *src[] = { "a" };
	char *dst[] = { "a", "b" };
	assert_false(is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));
}

static void
test_move_fail(void)
{
	char *src[] = { "a", "b" };
	char *dst[] = { "../a", "b" };
	assert_false(is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));

#ifdef _WIN32
	dst[0] = "..\\a";
	assert_false(is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));
#endif
}

static void
test_rename_inside_subdir_ok(void)
{
	char *src[] = { "../a", "b" };
	char *dst[] = { "../a_a", "b" };
	assert_true(is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));

#ifdef _WIN32
	src[0] = "..\\a";
	dst[0] = "..\\a_a";
	assert_true(is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));
#endif
}

void
rename_tests(void)
{
	test_fixture_start();

	run_test(test_names_less_than_files);
	run_test(test_names_greater_that_files_fail);
	run_test(test_move_fail);
	run_test(test_rename_inside_subdir_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
