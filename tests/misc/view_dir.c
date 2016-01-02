#include <stic.h>

#include <unistd.h> /* rmdir() */

#include <stdio.h> /* fclose() fopen() remove() */

#include "../../src/compat/os.h"
#include "../../src/ui/quickview.h"
#include "../../src/utils/string_array.h"

static void create_file(const char file[]);

SETUP()
{
	assert_success(chdir(SANDBOX_PATH));
}

TEST(file_can_not_be_viewed)
{
	assert_null(qv_view_dir(TEST_DATA_PATH "/existing-files/a"));
}

TEST(empty_dir_produces_single_line_and_dirs_have_trailing_slash)
{
	int nlines;
	FILE *fp;
	char **lines;

	assert_success(os_mkdir("empty-dir", 0777));

	fp = qv_view_dir("empty-dir");
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

	fp = qv_view_dir("dir");
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

	fp = qv_view_dir("dir");
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

	fp = qv_view_dir("dir");
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

	fp = qv_view_dir("dir");
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

	fp = qv_view_dir("dir");
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

static void
create_file(const char file[])
{
	FILE *const f = fopen(file, "w");
	if(f != NULL)
	{
		fclose(f);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
