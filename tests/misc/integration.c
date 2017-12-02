#include <stic.h>

#include <stdio.h> /* fclose() fmemopen() remove() */
#include <string.h> /* strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/int/vim.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

#include "utils.h"

/* Because of fmemopen(). */
#if !defined(_WIN32) && !defined(__APPLE__)

TEST(writing_directory_on_stdout_prints_newline)
{
	char out_buf[100] = { };
	char out_spec[] = "-";

	curr_stats.chosen_dir_out = out_spec;
	curr_stats.original_stdout = fmemopen(out_buf, sizeof(out_buf), "w");

	vim_write_dir("/some/path");

	fclose(curr_stats.original_stdout);
	curr_stats.original_stdout = NULL;
	curr_stats.chosen_dir_out = NULL;

	assert_string_equal("/some/path\n", out_buf);
}

TEST(writing_directory_to_a_file_prints_newline)
{
	char out_buf[100] = { };
	char out_spec[] = SANDBOX_PATH "/out";

	curr_stats.chosen_dir_out = out_spec;
	curr_stats.original_stdout = fmemopen(out_buf, sizeof(out_buf), "w");

	vim_write_dir("/some/path");

	fclose(curr_stats.original_stdout);
	curr_stats.original_stdout = NULL;
	curr_stats.chosen_dir_out = NULL;

	assert_int_equal(11, get_file_size(out_spec));
	assert_success(remove(out_spec));
}

TEST(paths_in_root_do_not_have_extra_slash_on_choosing)
{
	char file[] = "bin";
	char *files[] = { file };
	char out_buf[100] = { };
	char out_spec[] = "-";

	strcpy(lwin.curr_dir, "/");

	assert_success(stats_init(&cfg));

	curr_stats.chosen_files_out = out_spec;
	curr_stats.original_stdout = fmemopen(out_buf, sizeof(out_buf), "w");

	vim_write_file_list(&lwin, 1, files);

	fclose(curr_stats.original_stdout);
	curr_stats.original_stdout = NULL;
	curr_stats.chosen_files_out = NULL;

	assert_string_equal("/bin\n", out_buf);
}

TEST(abs_paths_are_not_touched_on_choosing)
{
	char file[] = "/bin";
	char *files[] = { file };
	char out_buf[100] = { };
	char out_spec[] = "-";

	strcpy(lwin.curr_dir, "/etc");

	assert_success(stats_init(&cfg));

	curr_stats.chosen_files_out = out_spec;
	curr_stats.original_stdout = fmemopen(out_buf, sizeof(out_buf), "w");

	vim_write_file_list(&lwin, 1, files);

	fclose(curr_stats.original_stdout);
	curr_stats.original_stdout = NULL;
	curr_stats.chosen_files_out = NULL;

	assert_string_equal("/bin\n", out_buf);
}

TEST(selection_can_be_chosen)
{
	char out_buf[100] = { };
	char out_spec[] = "-";

	char cwd[PATH_MAX + 1];
	char expected[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	view_setup(&lwin);

	strcpy(lwin.curr_dir, "/");
	strcpy(expected, "/bin\n");

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "bin");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
	lwin.dir_entry[0].selected = 1;

	assert_success(stats_init(&cfg));

	curr_stats.chosen_files_out = out_spec;
	curr_stats.original_stdout = fmemopen(out_buf, sizeof(out_buf), "w");

	vim_write_file_list(&lwin, 0, NULL);

	fclose(curr_stats.original_stdout);
	curr_stats.original_stdout = NULL;
	curr_stats.chosen_files_out = NULL;

	assert_string_equal(expected, out_buf);

	view_teardown(&lwin);
}

TEST(path_are_formed_right_on_choosing_in_cv)
{
	char file[] = "file";
	char *files[] = { file };
	char out_buf[100] = { };
	char out_spec[] = "-";

	char cwd[PATH_MAX + 1];
	char expected[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	view_setup(&lwin);

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", cwd);
	make_abs_path(expected, sizeof(expected), SANDBOX_PATH, "file\n", cwd);

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, ".");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(stats_init(&cfg));

	curr_stats.chosen_files_out = out_spec;
	curr_stats.original_stdout = fmemopen(out_buf, sizeof(out_buf), "w");

	vim_write_file_list(&lwin, 1, files);

	fclose(curr_stats.original_stdout);
	curr_stats.original_stdout = NULL;
	curr_stats.chosen_files_out = NULL;

	assert_string_equal(expected, out_buf);

	view_teardown(&lwin);
}

#endif

TEST(writing_directory_does_not_happen_with_empty_or_null_spec)
{
	char out_spec[] = "";

	curr_stats.chosen_dir_out = out_spec;
	vim_write_dir("/some/path");

	curr_stats.chosen_dir_out = NULL;
	vim_write_dir("/some/path");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
