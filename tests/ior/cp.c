#include "seatest.h"

#include <sys/stat.h> /* stat chmod() */
#include <sys/types.h> /* stat */
#include <unistd.h> /* F_OK access() lstat() */

#include "../../src/compat/os.h"
#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"
#include "utils.h"

static void
test_file_is_copied(void)
{
	{
		io_args_t args =
		{
			.arg1.src = "../read/binary-data",
			.arg2.dst = "binary-data",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "binary-data",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

static void
test_empty_directory_is_copied(void)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args =
		{
			.arg1.src = "empty-dir",
			.arg2.dst = "empty-dir-copy",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "empty-dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "empty-dir-copy",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

static void
test_non_empty_directory_is_copied(void)
{
	create_non_empty_dir("non-empty-dir", "a-file");

	{
		io_args_t args =
		{
			.arg1.src = "non-empty-dir",
			.arg2.dst = "non-empty-dir-copy",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	assert_int_equal(0, access("non-empty-dir-copy/a-file", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "non-empty-dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "non-empty-dir-copy",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_empty_nested_directory_is_copied(void)
{
	create_empty_nested_dir("non-empty-dir", "empty-nested-dir");

	{
		io_args_t args =
		{
			.arg1.src = "non-empty-dir",
			.arg2.dst = "non-empty-dir-copy",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	assert_int_equal(0,
			access("non-empty-dir-copy/empty-nested-dir", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "non-empty-dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "non-empty-dir-copy",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_non_empty_nested_directory_is_copied(void)
{
	create_non_empty_nested_dir("non-empty-dir", "nested-dir", "a-file");

	{
		io_args_t args =
		{
			.arg1.src = "non-empty-dir",
			.arg2.dst = "non-empty-dir-copy",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	assert_int_equal(0, access("non-empty-dir-copy/nested-dir/a-file", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "non-empty-dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "non-empty-dir-copy",
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
		assert_false(ior_cp(&args) == 0);
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
		assert_false(ior_cp(&args) == 0);
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
			.arg2.dst = "a-file",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_int_equal(0, ior_cp(&args));
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
			.arg2.dst = "dir",
			.arg3.crs = IO_CRS_REPLACE_ALL,
		};
		assert_int_equal(0, ior_cp(&args));
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
		assert_int_equal(0, ior_cp(&args));
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
test_fails_to_copy_directory_inside_itself(void)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args =
		{
			.arg1.src = "empty-dir",
			.arg2.dst = "empty-dir/empty-dir-copy",
		};
		assert_false(ior_cp(&args) == 0);
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
test_dir_permissions_are_preserved(void)
{
	struct stat src;
	struct stat dst;

	{
		io_args_t args =
		{
			.arg1.path = "dir",
			.arg3.mode = 0711,
		};
		assert_int_equal(0, iop_mkdir(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "dir",
			.arg2.dst = "dir-copy",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	assert_int_equal(0, os_stat("dir", &src));
	assert_int_equal(0, os_stat("dir-copy", &dst));
	assert_int_equal(src.st_mode & 0777, dst.st_mode & 0777);

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
			.arg1.path = "dir-copy",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

static void
test_permissions_are_set_in_correct_order(void)
{
	struct stat src;
	struct stat dst;

	{
		io_args_t args =
		{
			.arg1.path = "dir/nested-dir",
			.arg2.process_parents = 1,
			.arg3.mode = 0600,
		};
		assert_int_equal(0, iop_mkdir(&args));
	}

	assert_int_equal(0, chmod("dir", 0500));

	{
		io_args_t args =
		{
			.arg1.src = "dir",
			.arg2.dst = "dir-copy",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	assert_int_equal(0, os_stat("dir", &src));
	assert_int_equal(0, os_stat("dir-copy", &dst));
	assert_int_equal(src.st_mode & 0777, dst.st_mode & 0777);

	assert_int_equal(0, chmod("dir", 0700));
	assert_int_equal(0, chmod("dir-copy", 0700));

	{
		io_args_t args =
		{
			.arg1.path = "dir",
		};
		assert_int_equal(0, ior_rm(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "dir-copy",
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

#ifndef WIN32

static void
test_symlink_to_file_is_symlink_after_copy(void)
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
			.arg2.dst = "sym-link-copy",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	assert_true(is_symlink("sym-link"));
	assert_true(is_symlink("sym-link-copy"));

	{
		io_args_t args =
		{
			.arg1.path = "sym-link",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "sym-link-copy",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

static void
test_symlink_to_dir_is_symlink_after_copy(void)
{
	{
		io_args_t args =
		{
			.arg1.path = "../read",
			.arg2.target = "sym-link",
		};
		assert_int_equal(0, iop_ln(&args));
	}

	assert_true(is_symlink("sym-link"));

	{
		io_args_t args =
		{
			.arg1.src = "sym-link",
			.arg2.dst = "sym-link-copy",
		};
		assert_int_equal(0, ior_cp(&args));
	}

	assert_true(is_symlink("sym-link"));
	assert_true(is_symlink("sym-link-copy"));

	{
		io_args_t args =
		{
			.arg1.path = "sym-link",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "sym-link-copy",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

#endif

void
cp_tests(void)
{
	test_fixture_start();

	run_test(test_file_is_copied);
	run_test(test_empty_directory_is_copied);
	run_test(test_non_empty_directory_is_copied);
	run_test(test_empty_nested_directory_is_copied);
	run_test(test_non_empty_nested_directory_is_copied);
	run_test(test_fails_to_overwrite_file_by_default);
	run_test(test_fails_to_overwrite_dir_by_default);
	run_test(test_overwrites_file_when_asked);
	run_test(test_overwrites_dir_when_asked);
	run_test(test_directories_can_be_merged);
	run_test(test_fails_to_copy_directory_inside_itself);

	run_test(test_dir_permissions_are_preserved);
	run_test(test_permissions_are_set_in_correct_order);

#ifndef WIN32
	/* Creating symbolic links on Windows requires administrator rights. */
	run_test(test_symlink_to_file_is_symlink_after_copy);
	run_test(test_symlink_to_dir_is_symlink_after_copy);
#endif

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
