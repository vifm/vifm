#include <stic.h>

#include <sys/stat.h> /* stat chmod() */
#include <sys/types.h> /* stat */
#include <unistd.h> /* F_OK access() lstat() */

#include "../../src/compat/os.h"
#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/utils.h"

#include "utils.h"

static int not_windows(void);

TEST(file_is_copied)
{
	{
		io_args_t args = {
			.arg1.src = "../read/binary-data",
			.arg2.dst = "binary-data",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "binary-data",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(empty_directory_is_copied)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args = {
			.arg1.src = "empty-dir",
			.arg2.dst = "empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(non_empty_directory_is_copied)
{
	create_non_empty_dir("non-empty-dir", "a-file");

	{
		io_args_t args = {
			.arg1.src = "non-empty-dir",
			.arg2.dst = "non-empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access("non-empty-dir-copy/a-file", F_OK));

	{
		io_args_t args = {
			.arg1.path = "non-empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "non-empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(empty_nested_directory_is_copied)
{
	create_empty_nested_dir("non-empty-dir", "empty-nested-dir");

	{
		io_args_t args = {
			.arg1.src = "non-empty-dir",
			.arg2.dst = "non-empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access("non-empty-dir-copy/empty-nested-dir", F_OK));

	{
		io_args_t args = {
			.arg1.path = "non-empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "non-empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(non_empty_nested_directory_is_copied)
{
	create_non_empty_nested_dir("non-empty-dir", "nested-dir", "a-file");

	{
		io_args_t args = {
			.arg1.src = "non-empty-dir",
			.arg2.dst = "non-empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access("non-empty-dir-copy/nested-dir/a-file", F_OK));

	{
		io_args_t args = {
			.arg1.path = "non-empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "non-empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(fails_to_overwrite_file_by_default)
{
	create_empty_file("a-file");

	{
		io_args_t args = {
			.arg1.src = "../read/two-lines",
			.arg2.dst = "a-file",
		};
		ioe_errlst_init(&args.result.errors);

		assert_failure(ior_cp(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	{
		io_args_t args = {
			.arg1.path = "a-file",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(fails_to_overwrite_dir_by_default)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args = {
			.arg1.src = "../read",
			.arg2.dst = "empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_failure(ior_cp(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	{
		io_args_t args = {
			.arg1.path = "empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(overwrites_file_when_asked)
{
	create_empty_file("a-file");

	{
		io_args_t args = {
			.arg1.src = "../read/two-lines",
			.arg2.dst = "a-file",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "a-file",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(overwrites_dir_when_asked)
{
	create_empty_dir("dir");

	{
		io_args_t args = {
			.arg1.src = "../read",
			.arg2.dst = "dir",
			.arg3.crs = IO_CRS_REPLACE_ALL,
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_failure(iop_rmdir(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	{
		io_args_t args = {
			.arg1.path = "dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(directories_can_be_merged)
{
	create_empty_dir("first");

	assert_success(chdir("first"));
	create_empty_file("first-file");
	assert_success(chdir(".."));

	create_empty_dir("second");

	assert_success(chdir("second"));
	create_empty_file("second-file");
	assert_success(chdir(".."));

	{
		io_args_t args = {
			.arg1.src = "first",
			.arg2.dst = "second",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access("second/second-file", F_OK));
	assert_success(access("second/first-file", F_OK));

	{
		io_args_t args = {
			.arg1.path = "first",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "second",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(fails_to_copy_directory_inside_itself)
{
	create_empty_dir("empty-dir");

	{
		io_args_t args = {
			.arg1.src = "empty-dir",
			.arg2.dst = "empty-dir/empty-dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_failure(ior_cp(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	{
		io_args_t args = {
			.arg1.path = "empty-dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(dir_permissions_are_preserved)
{
	struct stat src;
	struct stat dst;

	{
		io_args_t args = {
			.arg1.path = "dir",
			.arg3.mode = 0711,
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_mkdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.src = "dir",
			.arg2.dst = "dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(os_stat("dir", &src));
	assert_success(os_stat("dir-copy", &dst));
	assert_int_equal(src.st_mode & 0777, dst.st_mode & 0777);

	{
		io_args_t args = {
			.arg1.path = "dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(permissions_are_set_in_correct_order)
{
	struct stat src;
	struct stat dst;

	{
		io_args_t args = {
			.arg1.path = "dir/nested-dir",
			.arg2.process_parents = 1,
			.arg3.mode = 0600,
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_mkdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(chmod("dir", 0500));

	{
		io_args_t args = {
			.arg1.src = "dir",
			.arg2.dst = "dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(os_stat("dir", &src));
	assert_success(os_stat("dir-copy", &dst));
	assert_int_equal(src.st_mode & 0777, dst.st_mode & 0777);

	assert_success(chmod("dir", 0700));
	assert_success(chmod("dir-copy", 0700));

	{
		io_args_t args = {
			.arg1.path = "dir",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "dir-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(symlink_to_file_is_symlink_after_copy, IF(not_windows))
{
	{
		io_args_t args = {
			.arg1.path = "../read/two-lines",
			.arg2.target = "sym-link",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_ln(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_true(is_symlink("sym-link"));

	{
		io_args_t args = {
			.arg1.src = "sym-link",
			.arg2.dst = "sym-link-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_true(is_symlink("sym-link"));
	assert_true(is_symlink("sym-link-copy"));

	{
		io_args_t args = {
			.arg1.path = "sym-link",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "sym-link-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(symlink_to_dir_is_symlink_after_copy, IF(not_windows))
{
	{
		io_args_t args = {
			.arg1.path = "../read",
			.arg2.target = "sym-link",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_ln(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_true(is_symlink("sym-link"));

	{
		io_args_t args = {
			.arg1.src = "sym-link",
			.arg2.dst = "sym-link-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_cp(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_true(is_symlink("sym-link"));
	assert_true(is_symlink("sym-link-copy"));

	{
		io_args_t args = {
			.arg1.path = "sym-link",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = "sym-link-copy",
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

static int
not_windows(void)
{
	return get_env_type() != ET_WIN;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
