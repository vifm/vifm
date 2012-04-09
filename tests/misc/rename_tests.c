#include "seatest.h"

#include "../../src/utils/macros.h"
#include "../../src/fileops.h"

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
rename_tests(void)
{
	test_fixture_start();

	run_test(test_names_less_than_files);
	run_test(test_names_greater_that_files_fail);
	run_test(test_move_fail);
	run_test(test_rename_inside_subdir_ok);
	run_test(test_incdec_leaves_zeros);

	run_test(test_substitute_segfault_bug);
	run_test(test_substitute_begin_global);
	run_test(test_substitute_end_global);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
