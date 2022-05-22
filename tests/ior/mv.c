#include <stic.h>

#include <sys/stat.h> /* chmod() */

#include <stdint.h> /* uint64_t */
#include <stdio.h> /* remove() rename() */
#include <string.h> /* strcmp() */

#include <test-utils.h>

#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/env.h"
#include "../../src/utils/fs.h"

#include "utils.h"

static IoErrCbResult ignore_errors(struct io_args_t *args,
		const ioe_err_t *err);
static int can_rename_changing_case(void);

TEST(file_is_moved)
{
	create_empty_file(SANDBOX_PATH "/binary-data");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/binary-data",
			.arg2.dst = SANDBOX_PATH "/moved-binary-data",
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	assert_false(file_exists(SANDBOX_PATH "/binary-data"));
	assert_true(file_exists(SANDBOX_PATH "/moved-binary-data"));

	remove(SANDBOX_PATH "/moved-binary-data");
}

TEST(empty_directory_is_moved)
{
	create_empty_dir(SANDBOX_PATH "/empty-dir");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/empty-dir",
			.arg2.dst = SANDBOX_PATH "/moved-empty-dir",
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	assert_false(is_dir(SANDBOX_PATH "/empty-dir"));
	assert_true(is_dir(SANDBOX_PATH "/moved-empty-dir"));

	delete_dir(SANDBOX_PATH "/moved-empty-dir");
}

TEST(non_empty_directory_is_moved)
{
	create_non_empty_dir(SANDBOX_PATH "/non-empty-dir", "a-file");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/non-empty-dir",
			.arg2.dst = SANDBOX_PATH "/moved-non-empty-dir",
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	assert_true(file_exists(SANDBOX_PATH "/moved-non-empty-dir/a-file"));

	delete_tree(SANDBOX_PATH "/moved-non-empty-dir");
}

TEST(empty_nested_directory_is_moved)
{
	create_empty_nested_dir(SANDBOX_PATH "/non-empty-dir", "empty-nested-dir");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/non-empty-dir",
			.arg2.dst = SANDBOX_PATH "/moved-non-empty-dir",
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	assert_true(is_dir(SANDBOX_PATH "/moved-non-empty-dir/empty-nested-dir"));

	delete_tree(SANDBOX_PATH "/moved-non-empty-dir");
}

TEST(non_empty_nested_directory_is_moved)
{
	create_non_empty_nested_dir(SANDBOX_PATH "/non-empty-dir", "nested-dir",
			"a-file");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/non-empty-dir",
			.arg2.dst = SANDBOX_PATH "/moved-non-empty-dir",
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	assert_false(file_exists(SANDBOX_PATH "/non-empty-dir/nested-dir/a-file"));
	assert_true(file_exists(SANDBOX_PATH
				"/moved-non-empty-dir/nested-dir/a-file"));

	delete_tree(SANDBOX_PATH "/moved-non-empty-dir");
}

TEST(fails_to_overwrite_file_by_default)
{
	create_empty_file(SANDBOX_PATH "/a-file");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read/two-lines",
			.arg2.dst = SANDBOX_PATH "/a-file",
		};
		assert_int_equal(IO_RES_FAILED, ior_mv(&args));
	}

	delete_file(SANDBOX_PATH "/a-file");
}

TEST(fails_to_overwrite_dir_by_default)
{
	create_empty_dir(SANDBOX_PATH "/empty-dir");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/empty-dir",
		};
		assert_int_equal(IO_RES_FAILED, ior_mv(&args));
	}

	delete_dir(SANDBOX_PATH "/empty-dir");
}

TEST(overwrites_file_when_asked)
{
	create_empty_file(SANDBOX_PATH "/a-file");
	clone_file(TEST_DATA_PATH "/read/two-lines", SANDBOX_PATH "/two-lines");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/two-lines",
			.arg2.dst = SANDBOX_PATH "/a-file",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	delete_file(SANDBOX_PATH "/a-file");
}

TEST(overwrites_dir_when_asked)
{
	create_empty_dir(SANDBOX_PATH "/dir");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/read",
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
	}

	assert_success(chmod(SANDBOX_PATH "/read", 0700));

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/dir",
			.arg3.crs = IO_CRS_REPLACE_ALL,
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/dir",
		};
		assert_int_equal(IO_RES_FAILED, iop_rmdir(&args));
	}

	delete_tree(SANDBOX_PATH "/dir");
}

TEST(appending_fails_for_directories)
{
	create_empty_dir(SANDBOX_PATH "/dir");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/read",
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_cp(&args));
	}

	assert_success(chmod(SANDBOX_PATH "/read", 0700));

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/dir",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_int_equal(IO_RES_FAILED, ior_mv(&args));
	}

	delete_dir(SANDBOX_PATH "/dir");

	delete_tree(SANDBOX_PATH "/read");
}

TEST(appending_works_for_files)
{
	uint64_t size;

	clone_file(TEST_DATA_PATH "/read/two-lines", SANDBOX_PATH "/two-lines");

	size = get_file_size(SANDBOX_PATH "/two-lines");

	clone_file(TEST_DATA_PATH "/read/two-lines", SANDBOX_PATH "/two-lines2");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/two-lines2",
			.arg2.dst = SANDBOX_PATH "/two-lines",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	assert_int_equal(size, get_file_size(SANDBOX_PATH "/two-lines"));

	delete_file(SANDBOX_PATH "/two-lines");
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
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	/* Original directory must be deleted. */
	assert_false(file_exists(SANDBOX_PATH "/first"));

	assert_true(file_exists(SANDBOX_PATH "/second/second-file"));
	assert_true(file_exists(SANDBOX_PATH "/second/first-file"));

	delete_tree(SANDBOX_PATH "/second");
}

TEST(nested_directories_can_be_merged)
{
	create_empty_dir(SANDBOX_PATH "/first");
	create_empty_dir(SANDBOX_PATH "/first/nested");
	create_empty_file(SANDBOX_PATH "/first/nested/first-file");

	create_empty_dir(SANDBOX_PATH "/second");
	create_empty_dir(SANDBOX_PATH "/second/nested");
	create_empty_file(SANDBOX_PATH "/second/nested/second-file");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/first",
			.arg2.dst = SANDBOX_PATH "/second",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	/* Original directory must be deleted. */
	assert_false(file_exists(SANDBOX_PATH "/first/nested"));
	assert_false(file_exists(SANDBOX_PATH "/first"));

	assert_true(file_exists(SANDBOX_PATH "/second/nested/second-file"));
	assert_true(file_exists(SANDBOX_PATH "/second/nested/first-file"));

	delete_tree(SANDBOX_PATH "/second");
}

TEST(fails_to_move_directory_inside_itself)
{
	create_empty_dir(SANDBOX_PATH "/empty-dir");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/empty-dir",
			.arg2.dst = SANDBOX_PATH "/empty-dir/empty-dir-copy",
		};
		assert_int_equal(IO_RES_FAILED, ior_mv(&args));
	}

	delete_dir(SANDBOX_PATH "/empty-dir");
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(symlink_is_symlink_after_move, IF(not_windows))
{
	{
		io_args_t args = {
			.arg1.path = TEST_DATA_PATH "/read/two-lines",
			.arg2.target = SANDBOX_PATH "/sym-link",
		};
		assert_int_equal(IO_RES_SUCCEEDED, iop_ln(&args));
	}

	assert_true(is_symlink(SANDBOX_PATH "/sym-link"));

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/sym-link",
			.arg2.dst = SANDBOX_PATH "/moved-sym-link",
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	assert_false(is_symlink(SANDBOX_PATH "/sym-link"));
	assert_true(is_symlink(SANDBOX_PATH "/moved-sym-link"));

	delete_file(SANDBOX_PATH "/moved-sym-link");
}

TEST(case_change_on_rename, IF(can_rename_changing_case))
{
	create_empty_file(SANDBOX_PATH "/a-file");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/a-file",
			.arg2.dst = SANDBOX_PATH "/A-file",
		};
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	}

	delete_file(SANDBOX_PATH "/A-file");
}

TEST(error_on_move_preserves_source_file, IF(regular_unix_user))
{
	io_args_t args = {
		.arg1.src = SANDBOX_PATH "/file",
		.arg2.dst = SANDBOX_PATH "/ro/file",

		.result.errors = IOE_ERRLST_INIT,
		.result.errors_cb = &ignore_errors,
	};

	create_empty_file(SANDBOX_PATH "/file");
	create_empty_dir(SANDBOX_PATH "/ro");
	assert_success(chmod(SANDBOX_PATH "/ro", 0500));

	assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
	assert_int_equal(1, args.result.errors.error_count);
	ioe_errlst_free(&args.result.errors);

	delete_file(SANDBOX_PATH "/file");
	delete_dir(SANDBOX_PATH "/ro");
}

static IoErrCbResult
ignore_errors(struct io_args_t *args, const ioe_err_t *err)
{
	return IO_ECR_IGNORE;
}

static int
can_rename_changing_case(void)
{
	int ok;
	create_empty_file(SANDBOX_PATH "/file");
	ok = rename(SANDBOX_PATH "/file", SANDBOX_PATH "/FiLe") == 0;
	if(ok)
	{
		delete_file(SANDBOX_PATH "/FiLe");
	}
	else
	{
		delete_file(SANDBOX_PATH "/file");
	}
	return ok;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
