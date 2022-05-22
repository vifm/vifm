#include <stic.h>

#include <sys/stat.h> /* stat chmod() */
#include <sys/types.h> /* stat */
#include <unistd.h> /* F_OK access() */

#include <test-utils.h>

#include "../../src/compat/os.h"
#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"

#include "utils.h"

TEST(file_is_copied)
{
	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read/binary-data",
			.arg2.dst = SANDBOX_PATH "/binary-data",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/binary-data",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(empty_directory_is_copied)
{
	create_empty_dir(SANDBOX_PATH "/empty-dir");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/empty-dir",
			.arg2.dst = SANDBOX_PATH "/empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(non_empty_directory_is_copied)
{
	create_non_empty_dir(SANDBOX_PATH "/non-empty-dir", "a-file");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/non-empty-dir",
			.arg2.dst = SANDBOX_PATH "/non-empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access(SANDBOX_PATH "/non-empty-dir-copy/a-file", F_OK));

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/non-empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/non-empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(empty_nested_directory_is_copied)
{
	create_empty_nested_dir(SANDBOX_PATH "/non-empty-dir", "empty-nested-dir");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/non-empty-dir",
			.arg2.dst = SANDBOX_PATH "/non-empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access(SANDBOX_PATH "/non-empty-dir-copy/empty-nested-dir",
				F_OK));

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/non-empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/non-empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(non_empty_nested_directory_is_copied)
{
	create_non_empty_nested_dir(SANDBOX_PATH "/non-empty-dir", "nested-dir",
			"a-file");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/non-empty-dir",
			.arg2.dst = SANDBOX_PATH "/non-empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access(SANDBOX_PATH "/non-empty-dir-copy/nested-dir/a-file",
				F_OK));

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/non-empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/non-empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(fails_to_overwrite_file_by_default)
{
	create_empty_file(SANDBOX_PATH "/a-file");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read/two-lines",
			.arg2.dst = SANDBOX_PATH "/a-file",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, ior_cp(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/a-file",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(fails_to_overwrite_dir_by_default)
{
	create_empty_dir(SANDBOX_PATH "/empty-dir");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, ior_cp(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(overwrites_file_when_asked)
{
	create_empty_file(SANDBOX_PATH "/a-file");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read/two-lines",
			.arg2.dst = SANDBOX_PATH "/a-file",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/a-file",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(overwrites_dir_when_asked)
{
	create_empty_dir(SANDBOX_PATH "/dir");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/dir",
			.arg3.crs = IO_CRS_REPLACE_ALL,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, iop_rmdir(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	assert_success(chmod(SANDBOX_PATH "/dir", 0700));

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(directories_can_be_merged)
{
	create_empty_dir(SANDBOX_PATH "/first");
	create_empty_file(SANDBOX_PATH "/first/first-file");

	create_empty_dir(SANDBOX_PATH "/second");
	create_empty_file(SANDBOX_PATH "/second/second-file");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/first",
			.arg2.dst = SANDBOX_PATH "/second",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access(SANDBOX_PATH "/second/second-file", F_OK));
	assert_success(access(SANDBOX_PATH "/second/first-file", F_OK));

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/first",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/second",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(fails_to_copy_directory_inside_itself)
{
	create_empty_dir(SANDBOX_PATH "/empty-dir");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/empty-dir",
			.arg2.dst = SANDBOX_PATH "/empty-dir/empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, ior_cp(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(dir_permissions_are_preserved)
{
	struct stat src;
	struct stat dst;

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/dir",
			.arg3.mode = 0711,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_mkdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/dir",
			.arg2.dst = SANDBOX_PATH "/dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(os_stat(SANDBOX_PATH "/dir", &src));
	assert_success(os_stat(SANDBOX_PATH "/dir-copy", &dst));
	assert_int_equal(src.st_mode & 0777, dst.st_mode & 0777);

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(permissions_are_set_in_correct_order)
{
	struct stat src;
	struct stat dst;

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/dir/nested-dir",
			.arg2.process_parents = 1,
			.arg3.mode = 0600,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_mkdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(chmod(SANDBOX_PATH "/dir", 0500));

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/dir",
			.arg2.dst = SANDBOX_PATH "/dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(os_stat(SANDBOX_PATH "/dir", &src));
	assert_success(os_stat(SANDBOX_PATH "/dir-copy", &dst));
	assert_int_equal(src.st_mode & 0777, dst.st_mode & 0777);

	assert_success(chmod(SANDBOX_PATH "/dir", 0700));
	assert_success(chmod(SANDBOX_PATH "/dir-copy", 0700));

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(symlink_to_file_is_symlink_after_copy, IF(not_windows))
{
	{
		io_args_t args = {
			.arg1.path = TEST_DATA_PATH "/read/two-lines",
			.arg2.target = SANDBOX_PATH "/sym-link",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_ln(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_true(is_symlink(SANDBOX_PATH "/sym-link"));

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/sym-link",
			.arg2.dst = SANDBOX_PATH "/sym-link-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_true(is_symlink(SANDBOX_PATH "/sym-link"));
	assert_true(is_symlink(SANDBOX_PATH "/sym-link-copy"));

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/sym-link",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/sym-link-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(symlink_to_dir_is_symlink_after_copy, IF(not_windows))
{
	{
		io_args_t args = {
			.arg1.path = TEST_DATA_PATH "/read",
			.arg2.target = SANDBOX_PATH "/sym-link",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_ln(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_true(is_symlink(SANDBOX_PATH "/sym-link"));

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/sym-link",
			.arg2.dst = SANDBOX_PATH "/sym-link-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_true(is_symlink(SANDBOX_PATH "/sym-link"));
	assert_true(is_symlink(SANDBOX_PATH "/sym-link-copy"));

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/sym-link",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/sym-link-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
