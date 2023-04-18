#include <stic.h>

#include <unistd.h> /* rmdir() */

#include <limits.h> /* INT_MAX */
#include <stdio.h> /* remove() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/ui/quickview.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/string_array.h"

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();

	assert_success(chdir(SANDBOX_PATH));
}

TEARDOWN()
{
	restore_cwd(saved_cwd);
}

TEST(file_can_not_be_viewed)
{
	assert_null(qv_view_dir(TEST_DATA_PATH "/existing-files/a", INT_MAX));
}

TEST(empty_dir_produces_single_line_and_dirs_have_trailing_slash)
{
	int nlines;
	FILE *fp;
	char **lines;

	assert_success(os_mkdir("empty-dir", 0777));

	fp = qv_view_dir("empty-dir", INT_MAX);
	lines = read_file_lines(fp, &nlines);

	assert_int_equal(3, nlines);
	assert_string_equal("empty-dir/", lines[0]);
	assert_string_equal("", lines[1]);
	assert_string_equal("0 directories, 0 files", lines[2]);

	free_string_array(lines, nlines);
	fclose(fp);

	assert_success(rmdir("empty-dir"));
}

TEST(single_file_is_displayed_correctly_file_without_slash)
{
	int nlines;
	FILE *fp;
	char **lines;

	assert_success(os_mkdir("dir", 0777));
	create_file("dir/file");

	fp = qv_view_dir("dir", INT_MAX);
	lines = read_file_lines(fp, &nlines);

	assert_int_equal(4, nlines);
	assert_string_equal("dir/", lines[0]);
	assert_string_equal("`-- file", lines[1]);
	assert_string_equal("", lines[2]);
	assert_string_equal("0 directories, 1 file", lines[3]);

	free_string_array(lines, nlines);
	fclose(fp);

	assert_success(remove("dir/file"));
	assert_success(rmdir("dir"));
}

TEST(single_subdir_is_displayed_correctly)
{
	int nlines;
	FILE *fp;
	char **lines;

	assert_success(os_mkdir("dir", 0777));
	assert_success(os_mkdir("dir/nested", 0777));

	fp = qv_view_dir("dir", INT_MAX);
	lines = read_file_lines(fp, &nlines);

	assert_int_equal(4, nlines);
	assert_string_equal("dir/", lines[0]);
	assert_string_equal("`-- nested/", lines[1]);
	assert_string_equal("", lines[2]);
	assert_string_equal("1 directory, 0 files", lines[3]);

	free_string_array(lines, nlines);
	fclose(fp);

	assert_success(rmdir("dir/nested"));
	assert_success(rmdir("dir"));
}

TEST(multiple_nested_dirs_treated_correctly)
{
	int nlines;
	FILE *fp;
	char **lines;

	assert_success(os_mkdir("dir", 0777));
	assert_success(os_mkdir("dir/nested1", 0777));
	assert_success(os_mkdir("dir/nested1/nested2", 0777));

	fp = qv_view_dir("dir", INT_MAX);
	lines = read_file_lines(fp, &nlines);

	assert_int_equal(5, nlines);
	assert_string_equal("dir/", lines[0]);
	assert_string_equal("`-- nested1/", lines[1]);
	assert_string_equal("    `-- nested2/", lines[2]);
	assert_string_equal("", lines[3]);
	assert_string_equal("2 directories, 0 files", lines[4]);

	free_string_array(lines, nlines);
	fclose(fp);

	assert_success(rmdir("dir/nested1/nested2"));
	assert_success(rmdir("dir/nested1"));
	assert_success(rmdir("dir"));
}

TEST(multiple_files_treated_correctly)
{
	int nlines;
	FILE *fp;
	char **lines;

	assert_success(os_mkdir("dir", 0777));
	create_file("dir/file1");
	create_file("dir/file2");

	fp = qv_view_dir("dir", INT_MAX);
	lines = read_file_lines(fp, &nlines);

	assert_int_equal(5, nlines);
	assert_string_equal("dir/", lines[0]);
	assert_string_equal("|-- file1", lines[1]);
	assert_string_equal("`-- file2", lines[2]);
	assert_string_equal("", lines[3]);
	assert_string_equal("0 directories, 2 files", lines[4]);

	free_string_array(lines, nlines);
	fclose(fp);

	assert_success(remove("dir/file2"));
	assert_success(remove("dir/file1"));
	assert_success(rmdir("dir"));
}

TEST(multiple_non_empty_dirs_have_correct_prefixes_plus_sorting)
{
	int nlines;
	FILE *fp;
	char **lines;

	assert_success(os_mkdir("dir", 0777));
	assert_success(os_mkdir("dir/sub1", 0777));
	assert_success(os_mkdir("dir/sub2", 0777));
	create_file("dir/sub1/file");
	create_file("dir/sub2/file");

	fp = qv_view_dir("dir", INT_MAX);
	lines = read_file_lines(fp, &nlines);

	assert_int_equal(7, nlines);
	assert_string_equal("dir/", lines[0]);
	assert_string_equal("|-- sub1/", lines[1]);
	assert_string_equal("|   `-- file", lines[2]);
	assert_string_equal("`-- sub2/", lines[3]);
	assert_string_equal("    `-- file", lines[4]);
	assert_string_equal("", lines[5]);
	assert_string_equal("2 directories, 2 files", lines[6]);

	free_string_array(lines, nlines);
	fclose(fp);

	assert_success(remove("dir/sub1/file"));
	assert_success(remove("dir/sub2/file"));
	assert_success(rmdir("dir/sub1"));
	assert_success(rmdir("dir/sub2"));
	assert_success(rmdir("dir"));
}

TEST(symlinks_are_not_resolved_in_tree_preview, IF(not_windows))
{
	int nlines;
	FILE *fp;
	char **lines;

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0777));
	assert_success(make_symlink(".", SANDBOX_PATH "/dir/link"));

	fp = qv_view_dir(SANDBOX_PATH "/dir", INT_MAX);
	lines = read_file_lines(fp, &nlines);

	assert_int_equal(4, nlines);
	assert_string_equal("dir/", lines[0]);
	assert_string_equal("`-- link/ -> .", lines[1]);
	assert_string_equal("", lines[2]);
	assert_string_equal("1 directory, 0 files", lines[3]);

	free_string_array(lines, nlines);
	fclose(fp);

	assert_success(unlink(SANDBOX_PATH "/dir/link"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

TEST(top_tree_stats)
{
	int i;
	int nlines;
	FILE *fp;
	char **lines;

	cfg.top_tree_stats = 1;

	assert_success(os_mkdir("dir", 0777));
	assert_success(os_mkdir("dir/nested1", 0777));
	assert_success(os_mkdir("dir/nested1/nested2", 0777));

	fp = qv_view_dir("dir", INT_MAX);
	lines = read_file_lines(fp, &nlines);

	assert_int_equal(5, nlines);
	assert_string_equal("2 directories, 0 files", lines[0]);
	for(i = 0; lines[1][i] != '\0'; ++i)
	{
		assert_true(lines[1][i] == ' ');
	}
	assert_string_equal("dir/", lines[2]);
	assert_string_equal("`-- nested1/", lines[3]);
	assert_string_equal("    `-- nested2/", lines[4]);

	free_string_array(lines, nlines);
	fclose(fp);

	assert_success(rmdir("dir/nested1/nested2"));
	assert_success(rmdir("dir/nested1"));
	assert_success(rmdir("dir"));

	cfg.top_tree_stats = 0;
}

TEST(top_tree_stats_in_small_window)
{
	int i;
	int nlines;
	FILE *fp;
	char **lines;
	char dir_path[PATH_MAX + 1];

	cfg.top_tree_stats = 1;

	make_abs_path(dir_path, sizeof(dir_path), TEST_DATA_PATH, "tree", saved_cwd);
	fp = qv_view_dir(dir_path, 5);
	lines = read_file_lines(fp, &nlines);

	assert_int_equal(6, nlines);
	assert_string_equal("5 directories, 7 files", lines[0]);
	for(i = 0; lines[1][i] != '\0'; ++i)
	{
		assert_true(lines[1][i] == ' ');
	}
	assert_string_equal("tree/", lines[2]);
	assert_string_equal("|-- .hidden", lines[3]);
	assert_string_equal("|-- dir1/", lines[4]);
	assert_string_equal("|   |-- dir2/", lines[5]);

	free_string_array(lines, nlines);
	fclose(fp);

	cfg.top_tree_stats = 0;
}

TEST(depth_limit)
{
	cfg.max_tree_depth = 2;

	assert_success(os_mkdir("dir", 0777));
	assert_success(os_mkdir("dir/nested1", 0777));
	assert_success(os_mkdir("dir/nested1/nested2", 0777));

	int nlines;
	FILE *fp = qv_view_dir("dir", INT_MAX);
	char **lines = read_file_lines(fp, &nlines);

	assert_int_equal(4, nlines);
	assert_string_equal("dir/", lines[0]);
	assert_string_equal("`-- nested1/", lines[1]);
	assert_string_equal("", lines[2]);
	assert_string_equal("1 directory, 0 files", lines[3]);

	free_string_array(lines, nlines);
	fclose(fp);

	assert_success(rmdir("dir/nested1/nested2"));
	assert_success(rmdir("dir/nested1"));
	assert_success(rmdir("dir"));

	cfg.max_tree_depth = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
