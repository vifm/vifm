#include <stic.h>

#include <stdint.h> /* uint64_t */
#include <stdio.h> /* remove() */

#include <unistd.h> /* chdir() */

#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/utils.h"

#include "utils.h"

static int not_windows(void);
static int windows(void);

TEST(file_is_moved)
{
	create_empty_file("binary-data");

	{
		io_args_t args = {
			.arg1.src = "binary-data",
			.arg2.dst = "moved-binary-data",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_false(file_exists("binary-data"));
	assert_true(file_exists("moved-binary-data"));

	remove("moved-binary-data");
}

TEST(empty_directory_is_moved)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args = {
			.arg1.src = "empty-dir",
			.arg2.dst = "moved-empty-dir",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_false(is_dir("empty-dir"));
	assert_true(is_dir("moved-empty-dir"));

	delete_dir("moved-empty-dir");
}

TEST(non_empty_directory_is_moved)
{
	create_non_empty_dir("non-empty-dir", "a-file");

	{
		io_args_t args = {
			.arg1.src = "non-empty-dir",
			.arg2.dst = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_true(file_exists("moved-non-empty-dir/a-file"));

	delete_tree("moved-non-empty-dir");
}

TEST(empty_nested_directory_is_moved)
{
	create_empty_nested_dir("non-empty-dir", "empty-nested-dir");

	{
		io_args_t args = {
			.arg1.src = "non-empty-dir",
			.arg2.dst = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_true(is_dir("moved-non-empty-dir/empty-nested-dir"));

	delete_tree("moved-non-empty-dir");
}

TEST(non_empty_nested_directory_is_moved)
{
	create_non_empty_nested_dir("non-empty-dir", "nested-dir", "a-file");

	{
		io_args_t args = {
			.arg1.src = "non-empty-dir",
			.arg2.dst = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_false(file_exists("non-empty-dir/nested-dir/a-file"));
	assert_true(file_exists("moved-non-empty-dir/nested-dir/a-file"));

	delete_tree("moved-non-empty-dir");
}

TEST(fails_to_overwrite_file_by_default)
{
	create_empty_file("a-file");

	{
		io_args_t args = {
			.arg1.src = "../read/two-lines",
			.arg2.dst = "a-file",
		};
		assert_false(ior_mv(&args) == 0);
	}

	delete_file("a-file");
}

TEST(fails_to_overwrite_dir_by_default)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args = {
			.arg1.src = "../read",
			.arg2.dst = "empty-dir",
		};
		assert_false(ior_mv(&args) == 0);
	}

	delete_dir("empty-dir");
}

TEST(overwrites_file_when_asked)
{
	create_empty_file("a-file");
	clone_file("../read/two-lines", "two-lines");

	{
		io_args_t args = {
			.arg1.src = "two-lines",
			.arg2.dst = "a-file",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	delete_file("a-file");
}

TEST(overwrites_dir_when_asked)
{
	create_empty_dir("dir");

	{
		io_args_t args = {
			.arg1.src = "../read",
			.arg2.dst = "read",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	{
		io_args_t args = {
			.arg1.src = "read",
			.arg2.dst = "dir",
			.arg3.crs = IO_CRS_REPLACE_ALL,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	{
		io_args_t args = {
			.arg1.path = "dir",
		};
		assert_false(iop_rmdir(&args) == 0);
	}

	delete_tree("dir");
}

TEST(appending_fails_for_directories)
{
	create_empty_dir("dir");

	{
		io_args_t args = {
			.arg1.src = "../read",
			.arg2.dst = "read",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	{
		io_args_t args = {
			.arg1.src = "read",
			.arg2.dst = "dir",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_false(ior_mv(&args) == 0);
	}

	delete_dir("dir");

	delete_tree("read");
}

TEST(appending_works_for_files)
{
	uint64_t size;

	clone_file("../read/two-lines", "two-lines");

	size = get_file_size("two-lines");

	clone_file("../read/two-lines", "two-lines2");

	{
		io_args_t args = {
			.arg1.src = "two-lines2",
			.arg2.dst = "two-lines",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_int_equal(size, get_file_size("two-lines"));

	delete_file("two-lines");
}

TEST(directories_can_be_merged)
{
	create_empty_dir("first");

	assert_int_equal(0, chdir("first"));
	create_empty_file("first-file");
	assert_int_equal(0, chdir(".."));

	create_empty_dir("second");

	assert_int_equal(0, chdir("second"));
	create_empty_file("second-file");
	assert_int_equal(0, chdir(".."));

	{
		io_args_t args = {
			.arg1.src = "first",
			.arg2.dst = "second",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_true(file_exists("second/second-file"));
	assert_true(file_exists("second/first-file"));

	delete_tree("first");
	delete_tree("second");
}

TEST(fails_to_move_directory_inside_itself)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args = {
			.arg1.src = "empty-dir",
			.arg2.dst = "empty-dir/empty-dir-copy",
		};
		assert_false(ior_mv(&args) == 0);
	}

	delete_dir("empty-dir");
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(symlink_is_symlink_after_move, IF(not_windows))
{
	{
		io_args_t args = {
			.arg1.path = "../read/two-lines",
			.arg2.target = "sym-link",
		};
		assert_int_equal(0, iop_ln(&args));
	}

	assert_true(is_symlink("sym-link"));

	{
		io_args_t args = {
			.arg1.src = "sym-link",
			.arg2.dst = "moved-sym-link",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_true(!is_symlink("sym-link"));
	assert_true(is_symlink("moved-sym-link"));

	delete_file("moved-sym-link");
}

/* Case insensitive renames are easier to check on Windows. */
TEST(case_insensitive_rename, IF(windows))
{
	create_empty_file("a-file");

	{
		io_args_t args = {
			.arg1.src = "a-file",
			.arg2.dst = "A-file",
		};
		assert_true(ior_mv(&args) == 0);
	}

	delete_file("A-file");
}

static int
not_windows(void)
{
	return get_env_type() != ET_WIN;
}

static int
windows(void)
{
	return get_env_type() == ET_WIN;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
