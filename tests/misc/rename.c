#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stddef.h> /* size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* memset() strdup() strlen() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/int/ext_edit.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/macros.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/fops_common.h"
#include "../../src/fops_rename.h"

static void check_editing(char *orig[], int orig_len, const char template[],
		const char edited[], char *expected[], int expected_len);

static ext_edit_t ext_edit;

SETUP_ONCE()
{
	curr_view = &lwin;
}

TEARDOWN()
{
	ext_edit_discard(&ext_edit);
}

TEST(names_less_than_files)
{
	char *src[] = { "a", "b" };
	char *dst[] = { "a" };
	char *error = NULL;
	assert_false(fops_is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst,
				&error));
	assert_string_equal("Not enough file names (1/2)", error);
	free(error);
}

TEST(names_greater_that_files_fail)
{
	char *src[] = { "a" };
	char *dst[] = { "a", "b" };
	char *error = NULL;
	assert_false(fops_is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst,
				&error));
	assert_string_equal("Too many file names (2/1)", error);
	free(error);
}

TEST(move_fail)
{
	char *src[] = { "a", "b" };
	char *dst[] = { "../a", "b" };
	char *error = NULL;
	assert_false(fops_is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst,
				&error));
	assert_string_equal("Won't move \"../a\" file", error);

#ifdef _WIN32
	dst[0] = "..\\a";
	assert_false(fops_is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst,
				&error));
	assert_string_equal("Won't move \"..\\a\" file", error);
#endif

	free(error);
}

TEST(rename_inside_subdir_ok)
{
	char *src[] = { "../a", "b" };
	char *dst[] = { "../a_a", "b" };
	char *error = NULL;
	assert_true(fops_is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst,
				&error));
	assert_string_equal(NULL, error);

#ifdef _WIN32
	src[0] = "..\\a";
	dst[0] = "..\\a_a";
	assert_true(fops_is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst,
			&error));
	assert_string_equal(NULL, error);
#endif

	free(error);
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
	char *error = NULL;

	assert_success(chdir(TEST_DATA_PATH "/rename"));
	assert_true(fops_check_file_rename(".", "a", "a", &error) < 0);
	assert_string_equal(NULL, error);
	assert_true(fops_check_file_rename(".", "a", "", &error) < 0);
	assert_string_equal(NULL, error);
	assert_true(fops_check_file_rename(".", "a", "b", &error) > 0);
	assert_string_equal(NULL, error);
	assert_true(fops_check_file_rename(".", "a", "aa", &error) == 0);
	assert_string_equal("File \"aa\" already exists", error);
#ifdef _WIN32
	update_string(&error, NULL);
	assert_true(fops_check_file_rename(".", "a", "A", &error) > 0);
	assert_string_equal(NULL, error);
#endif

	free(error);
}

TEST(rename_list_checks)
{
	size_t i;
	char *list[] = { "a", "aa", "aaa" };
	char *files[] = { "", "aa", "bbb" };
	ARRAY_GUARD(files, ARRAY_LEN(list));
	char dup[ARRAY_LEN(files)] = {};
	char *error = NULL;

	assert_true(fops_is_rename_list_ok(files, dup, ARRAY_LEN(list), list,
				&error));
	assert_string_equal(NULL, error);
	free(error);

	for(i = 0; i < ARRAY_LEN(list); ++i)
	{
		assert_false(dup[i]);
	}
}

TEST(file_name_list_can_be_reread)
{
	char long_name[PATH_MAX + NAME_MAX + 1];
	char *list[] = { "aaa", long_name };
	int nlines;
	char **new_list;

	memset(long_name, '1', sizeof(long_name) - 1U);
	long_name[sizeof(long_name) - 1U] = '\0';

#ifndef _WIN32
	replace_string(&cfg.shell, "/bin/sh");
	update_string(&cfg.vi_command, "echo > /dev/null");
#else
	replace_string(&cfg.shell, "cmd");
	update_string(&cfg.vi_command, "echo > NUL");
#endif
	stats_update_shell_type(cfg.shell);

	curr_stats.exec_env_type = EET_EMULATOR;

	ext_edit_t ext_edit = {};

	new_list = edit_list(&ext_edit, ARRAY_LEN(list), list, &nlines, 1);
	assert_int_equal(2, nlines);
	if(nlines >= 2)
	{
		assert_string_equal(list[0], new_list[0]);
		assert_string_equal(list[1], new_list[1]);
	}
	free_string_array(new_list, nlines);

	ext_edit_discard(&ext_edit);

	update_string(&cfg.vi_command, NULL);

	update_string(&cfg.shell, NULL);
	stats_update_shell_type("/bin/sh");
}

TEST(file_name_list_can_be_changed, IF(not_windows))
{
	char *orig[] = { "aaa" };
	const char *edited = "bbb";
	char *final[] = { "bbb" };
	check_editing(orig, ARRAY_LEN(orig), NULL, edited, final, ARRAY_LEN(final));
}

TEST(leading_chars_and_comments, IF(not_windows))
{
	char *orig[] = { "#aa", "\\escapeme" };
	const char *edited = "\\\\aa\n"
	                     "#commenthere\n"
	                     "\\#escapeme\n"
	                     "# and some\n"
	                     "# more here\n";
	char *final[] = { "\\aa", "#escapeme" };
	check_editing(orig, ARRAY_LEN(orig), NULL, edited, final, ARRAY_LEN(final));
}

TEST(re_editing, IF(not_windows))
{
	char *orig[] = { "first" };
	const char *edited = "second";
	char *final[] = { "second" };
	check_editing(orig, ARRAY_LEN(orig), NULL, edited, final, ARRAY_LEN(final));
	check_editing(orig, ARRAY_LEN(orig), NULL, NULL, final, ARRAY_LEN(final));
}

TEST(re_editing_cancellation, IF(not_windows))
{
	char *orig[] = { "first" };
	const char *edited = "second";
	const char *cancel = "#only comments";
	char *modified[] = { "second" };
	check_editing(orig, ARRAY_LEN(orig), NULL, edited, modified,
			ARRAY_LEN(modified));
	check_editing(orig, ARRAY_LEN(orig), NULL, cancel, NULL, 0);
	check_editing(orig, ARRAY_LEN(orig), NULL, NULL, orig, ARRAY_LEN(orig));
}

TEST(re_editing_unchanged_skips_caching, IF(not_windows))
{
	char *orig[] = { "a", "b" };
	const char *same = "a\nb";
	check_editing(orig, ARRAY_LEN(orig), same, same, NULL, 0);
	check_editing(orig, ARRAY_LEN(orig), same, same, NULL, 0);
}

TEST(last_rename_error_is_displayed, IF(not_windows))
{
	char *orig[] = { "a", "b" };
	const char *template = "# Last error:\n"
	                       "#  test error\n"
	                       "# Original names:\n"
	                       "#  a\n"
	                       "#  b\n"
	                       "# Edited names:\n"
	                       "a\n"
	                       "a\n";
	const char *edited = "a\na";
	char *modified[] = { "a", "a" };
	check_editing(orig, ARRAY_LEN(orig), NULL, edited, modified,
			ARRAY_LEN(modified));
	update_string(&ext_edit.last_error, "test error");
	check_editing(orig, ARRAY_LEN(orig), template, edited, modified,
			ARRAY_LEN(modified));
}

TEST(unchanged_list, IF(not_windows))
{
	char *orig[] = { "aaa" };
	const char *edited = "aaa";
	check_editing(orig, ARRAY_LEN(orig), NULL, edited, NULL, 0);
}

static void
check_editing(char *orig[], int orig_len, const char template[],
		const char edited[], char *expected[], int expected_len)
{
	char *saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	update_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type(cfg.shell);

	create_executable("script");
	if(edited != NULL)
	{
		make_file("edited", edited);
		make_file("script", "#!/bin/sh\n"
		                    "mv $2 template\n"
		                    "mv edited $2\n");
	}

	curr_stats.exec_env_type = EET_EMULATOR;
	update_string(&cfg.vi_command, "./script");

	int actual_len;
	char **actual = edit_list(&ext_edit, orig_len, orig, &actual_len,
			edited == NULL);
	assert_int_equal(expected_len, actual_len);

	int i;
	for(i = 0; i < MIN(expected_len, actual_len); ++i)
	{
		assert_string_equal(expected[i], actual[i]);
	}

	free_string_array(actual, actual_len);

	update_string(&cfg.vi_command, NULL);
	update_string(&cfg.shell, NULL);

	if(template != NULL)
	{
		char *template_copy = strdup(template);
		int nlines;
		char **lines = break_into_lines(template_copy, strlen(template_copy),
				&nlines, 0);
		file_is("template", (const char **)lines, nlines);
		free_string_array(lines, nlines);
		free(template_copy);
	}
	if(edited != NULL)
	{
		remove_file("template");
	}

	remove_file("script");

	restore_cwd(saved_cwd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
