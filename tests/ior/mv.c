#include "seatest.h"

#include <stdint.h> /* uint64_t */
#include <stdio.h> /* remove() */

#include <unistd.h> /* F_OK access() */

#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"
#include "utils.h"

static void
test_file_is_moved(void)
{
	create_empty_file("binary-data");

	{
		io_args_t args =
		{
			.arg1.src = "binary-data",
			.arg2.dst = "moved-binary-data",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_false(access("binary-data", F_OK) == 0);
	assert_int_equal(0, access("moved-binary-data", F_OK));

	remove("moved-binary-data");
}

static void
test_empty_directory_is_moved(void)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args =
		{
			.arg1.src = "empty-dir",
			.arg2.dst = "moved-empty-dir",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_false(is_dir("empty-dir"));
	assert_true(is_dir("moved-empty-dir"));

	{
		io_args_t args =
		{
			.arg1.path = "moved-empty-dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

static void
test_non_empty_directory_is_moved(void)
{
	create_non_empty_dir("non-empty-dir", "a-file");

	{
		io_args_t args =
		{
			.arg1.src = "non-empty-dir",
			.arg2.dst = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_int_equal(0, access("moved-non-empty-dir/a-file", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_empty_nested_directory_is_moved(void)
{
	create_empty_nested_dir("non-empty-dir", "empty-nested-dir");

	{
		io_args_t args =
		{
			.arg1.src = "non-empty-dir",
			.arg2.dst = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_true(is_dir("moved-non-empty-dir/empty-nested-dir"));

	{
		io_args_t args =
		{
			.arg1.path = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_non_empty_nested_directory_is_moved(void)
{
	create_non_empty_nested_dir("non-empty-dir", "nested-dir", "a-file");

	{
		io_args_t args =
		{
			.arg1.src = "non-empty-dir",
			.arg2.dst = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_false(access("non-empty-dir/nested-dir/a-file", F_OK) == 0);
	assert_int_equal(0, access("moved-non-empty-dir/nested-dir/a-file", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "moved-non-empty-dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_fails_to_overwrite_file_by_default(void)
{
	create_empty_file("a-file");

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "a-file",
		};
		assert_false(ior_mv(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "a-file",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

static void
test_fails_to_overwrite_dir_by_default(void)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args =
		{
			.arg1.src = "../read",
			.arg2.dst = "empty-dir",
		};
		assert_false(ior_mv(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "empty-dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

static void
test_overwrites_file_when_asked(void)
{
	create_empty_file("a-file");

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "two-lines",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "two-lines",
			.arg2.dst = "a-file",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "a-file",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

static void
test_overwrites_dir_when_asked(void)
{
	create_empty_dir("dir");

	{
		io_args_t args =
		{
			.arg1.src = "../read",
			.arg2.dst = "read",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "read",
			.arg2.dst = "dir",
			.arg3.crs = IO_CRS_REPLACE_ALL,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "dir",
		};
		assert_false(iop_rmdir(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_appending_fails_for_directories(void)
{
	create_empty_dir("dir");

	{
		io_args_t args =
		{
			.arg1.src = "../read",
			.arg2.dst = "read",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "read",
			.arg2.dst = "dir",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_false(ior_mv(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "read",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_appending_works_for_files(void)
{
	uint64_t size;

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "two-lines",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	size = get_file_size("two-lines");

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "two-lines2",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "two-lines2",
			.arg2.dst = "two-lines",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_int_equal(size, get_file_size("two-lines"));

	{
		io_args_t args =
		{
			.arg1.path = "two-lines",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

static void
test_directories_can_be_merged(void)
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
		io_args_t args =
		{
			.arg1.src = "first",
			.arg2.dst = "second",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_int_equal(0, access("second/second-file", F_OK));
	assert_int_equal(0, access("second/first-file", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "first",
		};
		assert_int_equal(0, ior_rm(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "second",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_fails_to_move_directory_inside_itself(void)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args =
		{
			.arg1.src = "empty-dir",
			.arg2.dst = "empty-dir/empty-dir-copy",
		};
		assert_false(ior_mv(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "empty-dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

#ifndef _WIN32

static void
test_symlink_is_symlink_after_move(void)
{
	{
		io_args_t args =
		{
			.arg1.path = "../read/two-lines",
			.arg2.target = "sym-link",
		};
		assert_int_equal(0, iop_ln(&args));
	}

	assert_true(is_symlink("sym-link"));

	{
		io_args_t args =
		{
			.arg1.src = "sym-link",
			.arg2.dst = "moved-sym-link",
		};
		assert_int_equal(0, ior_mv(&args));
	}

	assert_true(!is_symlink("sym-link"));
	assert_true(is_symlink("moved-sym-link"));

	{
		io_args_t args =
		{
			.arg1.path = "moved-sym-link",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

#else

static void
test_case_insensitive_rename(void)
{
	create_empty_file("a-file");

	{
		io_args_t args =
		{
			.arg1.src = "a-file",
			.arg2.dst = "A-file",
		};
		assert_true(ior_mv(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = "A-file",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

#endif

void
mv_tests(void)
{
	test_fixture_start();

	run_test(test_file_is_moved);
	run_test(test_empty_directory_is_moved);
	run_test(test_non_empty_directory_is_moved);
	run_test(test_empty_nested_directory_is_moved);
	run_test(test_non_empty_nested_directory_is_moved);
	run_test(test_fails_to_overwrite_file_by_default);
	run_test(test_fails_to_overwrite_dir_by_default);
	run_test(test_overwrites_file_when_asked);
	run_test(test_overwrites_dir_when_asked);
	run_test(test_appending_fails_for_directories);
	run_test(test_appending_works_for_files);
	run_test(test_directories_can_be_merged);
	run_test(test_fails_to_move_directory_inside_itself);

#ifndef _WIN32
	/* Creating symbolic links on Windows requires administrator rights. */
	run_test(test_symlink_is_symlink_after_move);
#else
	/* Case insensitive renames are easier to check on Windows. */
	run_test(test_case_insensitive_rename);
#endif

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
