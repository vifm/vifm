#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* chdir() */

#include <stddef.h> /* size_t */
#include <string.h> /* memset() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/macros.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/fops_common.h"
#include "../../src/fops_rename.h"

SETUP_ONCE()
{
	curr_view = &lwin;
}

TEST(names_less_than_files)
{
	char *src[] = { "a", "b" };
	char *dst[] = { "a" };
	assert_false(fops_is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));
}

TEST(names_greater_that_files_fail)
{
	char *src[] = { "a" };
	char *dst[] = { "a", "b" };
	assert_false(fops_is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));
}

TEST(move_fail)
{
	char *src[] = { "a", "b" };
	char *dst[] = { "../a", "b" };
	assert_false(fops_is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));

#ifdef _WIN32
	dst[0] = "..\\a";
	assert_false(fops_is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));
#endif
}

TEST(rename_inside_subdir_ok)
{
	char *src[] = { "../a", "b" };
	char *dst[] = { "../a_a", "b" };
	assert_true(fops_is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));

#ifdef _WIN32
	src[0] = "..\\a";
	dst[0] = "..\\a_a";
	assert_true(fops_is_name_list_ok(ARRAY_LEN(src), ARRAY_LEN(dst), src, dst));
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
	assert_true(fops_check_file_rename(".", "a", "a", ST_NONE) < 0);
	assert_true(fops_check_file_rename(".", "a", "", ST_NONE) < 0);
	assert_true(fops_check_file_rename(".", "a", "b", ST_NONE) > 0);
	assert_true(fops_check_file_rename(".", "a", "aa", ST_NONE) == 0);
#ifdef _WIN32
	assert_true(fops_check_file_rename(".", "a", "A", ST_NONE) > 0);
#endif
}

TEST(rename_list_checks)
{
	size_t i;
	char *list[] = { "a", "aa", "aaa" };
	char *files[] = { "", "aa", "bbb" };
	ARRAY_GUARD(files, ARRAY_LEN(list));
	char dup[ARRAY_LEN(files)] = {};

	assert_true(fops_is_rename_list_ok(files, dup, ARRAY_LEN(list), list));
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

	new_list = fops_edit_list(ARRAY_LEN(list), list, &nlines, 1);
	assert_int_equal(2, nlines);
	if(nlines >= 2)
	{
		assert_string_equal(list[0], new_list[0]);
		assert_string_equal(list[1], new_list[1]);
	}
	free_string_array(new_list, nlines);

	update_string(&cfg.vi_command, NULL);

	update_string(&cfg.shell, NULL);
	stats_update_shell_type("/bin/sh");
}

TEST(file_name_list_can_be_changed, IF(not_windows))
{
	char *list[] = { "aaa" };
	int nlines;
	char **new_list;
	FILE *fp;

	char *saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	update_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type(cfg.shell);

	fp = fopen("script", "w");
	fputs("#!/bin/sh\n", fp);
	fputs("sed 'y/a/b/' < $2 > $2_out\n", fp);
	fputs("mv $2_out $2\n", fp);
	fclose(fp);
	assert_success(chmod("script", 0777));

	curr_stats.exec_env_type = EET_EMULATOR;
	update_string(&cfg.vi_command, "./script");

	new_list = fops_edit_list(ARRAY_LEN(list), list, &nlines, 0);
	assert_int_equal(1, nlines);
	if(nlines >= 1)
	{
		assert_string_equal("bbb", new_list[0]);
	}
	free_string_array(new_list, nlines);

	update_string(&cfg.vi_command, NULL);

	update_string(&cfg.shell, NULL);

	assert_success(unlink("script"));

	restore_cwd(saved_cwd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
