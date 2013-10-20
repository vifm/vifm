#include "seatest.h"

#include "../../src/utils/macros.h"
#include "../../src/fileops.h"
#include "../../src/ui.h"

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

static void
test_incdec_leaves_zeros(void)
{
	assert_string_equal("1", add_to_name("0", 1));
	assert_string_equal("01", add_to_name("00", 1));
	assert_string_equal("00", add_to_name("01", -1));
	assert_string_equal("-01", add_to_name("00", -1));
	assert_string_equal("002", add_to_name("001", 1));
	assert_string_equal("012", add_to_name("005", 7));
	assert_string_equal("008", add_to_name("009", -1));
	assert_string_equal("010", add_to_name("009", 1));
	assert_string_equal("100", add_to_name("099", 1));
	assert_string_equal("-08", add_to_name("-09", 1));
	assert_string_equal("-10", add_to_name("-09", -1));
	assert_string_equal("-14", add_to_name("-09", -5));
	assert_string_equal("a01.", add_to_name("a00.", 1));
}

static void
test_single_file_rename(void)
{
	chdir("test-data/rename");
	assert_true(check_file_rename(".", "a", "a", ST_NONE) < 0);
	assert_true(check_file_rename(".", "a", "", ST_NONE) < 0);
	assert_true(check_file_rename(".", "a", "b", ST_NONE) > 0);
	assert_true(check_file_rename(".", "a", "aa", ST_NONE) == 0);
#ifdef _WIN32
	assert_true(check_file_rename(".", "a", "A", ST_NONE) > 0);
#endif
	chdir("../..");
}

static void
test_rename_list_checks(void)
{
	int i;
	char *list[] = { "a", "aa", "aaa" };
	char *files[] = { "", "aa", "bbb" };
	ARRAY_GUARD(files, ARRAY_LEN(list));
	int dup[ARRAY_LEN(files)] = {};

	assert_true(is_rename_list_ok(files, dup, ARRAY_LEN(list), list));
	for(i = 0; i < ARRAY_LEN(list); i++)
	{
		assert_false(dup[i]);
	}
}

void
rename_tests(void)
{
	test_fixture_start();

	run_test(test_names_less_than_files);
	run_test(test_names_greater_that_files_fail);
	run_test(test_move_fail);
	run_test(test_rename_inside_subdir_ok);
	run_test(test_incdec_leaves_zeros);
	run_test(test_single_file_rename);
	run_test(test_rename_list_checks);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
