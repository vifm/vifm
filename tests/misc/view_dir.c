#include <stic.h>

#include <stdio.h> /* fclose() fopen() */
#include <stdlib.h> /* remove() */

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

	assert_int_equal(1, nlines);
	assert_string_equal("empty-dir/", lines[0]);

	free_string_array(lines, nlines);
	fclose(fp);

	remove("empty-dir");
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

	assert_int_equal(2, nlines);
	assert_string_equal("dir/", lines[0]);
	assert_string_equal("`-- file", lines[1]);

	free_string_array(lines, nlines);
	fclose(fp);

	remove("dir/file");
	remove("dir");
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

	assert_int_equal(2, nlines);
	assert_string_equal("dir/", lines[0]);
	assert_string_equal("`-- nested/", lines[1]);

	free_string_array(lines, nlines);
	fclose(fp);

	remove("dir/nested");
	remove("dir");
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

	assert_int_equal(3, nlines);
	assert_string_equal("dir/", lines[0]);
	assert_string_equal("`-- nested1/", lines[1]);
	assert_string_equal("    `-- nested2/", lines[2]);

	free_string_array(lines, nlines);
	fclose(fp);

	remove("dir/nested1/nested2");
	remove("dir/nested1");
	remove("dir");
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

	assert_int_equal(3, nlines);
	assert_string_equal("dir/", lines[0]);
	assert_string_equal("|-- file1", lines[1]);
	assert_string_equal("`-- file2", lines[2]);

	free_string_array(lines, nlines);
	fclose(fp);

	remove("dir/file2");
	remove("dir/file1");
	remove("dir");
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

	assert_int_equal(5, nlines);
	assert_string_equal("dir/", lines[0]);
	assert_string_equal("|-- sub1/", lines[1]);
	assert_string_equal("|   `-- file", lines[2]);
	assert_string_equal("`-- sub2/", lines[3]);
	assert_string_equal("    `-- file", lines[4]);

	free_string_array(lines, nlines);
	fclose(fp);

	remove("dir/sub1/file");
	remove("dir/sub2/file");
	remove("dir/sub1");
	remove("dir/sub2");
	remove("dir");
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
