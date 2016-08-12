#include <stic.h>

#include <unistd.h> /* chdir() */

#include "../../src/ui/ui.h"
#include "../../src/utils/macros.h"
#include "../../src/fileops.h"

TEST(names_less_than_files)
{
	char *src[] = { "a", "b" };
	char *dst[] = { "a" };
	assert_false(is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));
}

TEST(names_greater_that_files_fail)
{
	char *src[] = { "a" };
	char *dst[] = { "a", "b" };
	assert_false(is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));
}

TEST(move_fail)
{
	char *src[] = { "a", "b" };
	char *dst[] = { "../a", "b" };
	assert_false(is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));

#ifdef _WIN32
	dst[0] = "..\\a";
	assert_false(is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));
#endif
}

TEST(rename_inside_subdir_ok)
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

TEST(incdec_leaves_zeros)
{
	assert_string_equal("1", incdec_name("0", 1));
	assert_string_equal("01", incdec_name("00", 1));
	assert_string_equal("00", incdec_name("01", -1));
	assert_string_equal("-01", incdec_name("00", -1));
	assert_string_equal("002", incdec_name("001", 1));
	assert_string_equal("012", incdec_name("005", 7));
	assert_string_equal("008", incdec_name("009", -1));
	assert_string_equal("010", incdec_name("009", 1));
	assert_string_equal("100", incdec_name("099", 1));
	assert_string_equal("-08", incdec_name("-09", 1));
	assert_string_equal("-10", incdec_name("-09", -1));
	assert_string_equal("-14", incdec_name("-09", -5));
	assert_string_equal("a01.", incdec_name("a00.", 1));
}

TEST(single_file_rename)
{
	assert_success(chdir(TEST_DATA_PATH "/rename"));
	assert_true(check_file_rename(".", "a", "a", ST_NONE) < 0);
	assert_true(check_file_rename(".", "a", "", ST_NONE) < 0);
	assert_true(check_file_rename(".", "a", "b", ST_NONE) > 0);
	assert_true(check_file_rename(".", "a", "aa", ST_NONE) == 0);
#ifdef _WIN32
	assert_true(check_file_rename(".", "a", "A", ST_NONE) > 0);
#endif
}

TEST(rename_list_checks)
{
	int i;
	char *list[] = { "a", "aa", "aaa" };
	char *files[] = { "", "aa", "bbb" };
	ARRAY_GUARD(files, ARRAY_LEN(list));
	char dup[ARRAY_LEN(files)] = {};

	assert_true(is_rename_list_ok(files, dup, ARRAY_LEN(list), list));
	for(i = 0; i < ARRAY_LEN(list); i++)
	{
		assert_false(dup[i]);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
