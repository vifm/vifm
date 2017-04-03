#include <stic.h>

#include <stdio.h> /* fclose() fmemopen() remove() */

#include "../../src/int/vim.h"
#include "../../src/utils/fs.h"
#include "../../src/status.h"

/* Because of fmemopen(). */
#ifndef _WIN32

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
