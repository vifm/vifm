#include <stic.h>

#include <stdint.h> /* uint64_t */
#include <stdio.h> /* remove() */

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* F_OK access() */

#include <test-utils.h>

#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/utils.h"

#include "utils.h"

static int confirm_overwrite(io_args_t *args, const char src[],
		const char dst[]);
static int deny_overwrite(io_args_t *args, const char src[], const char dst[]);

static int confirm_called;

TEST(confirm_is_not_called_for_no_overwrite)
{
	create_empty_file(SANDBOX_PATH "/empty");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read/two-lines",
			.arg2.dst = SANDBOX_PATH "/empty",
			.arg3.crs = IO_CRS_FAIL,

			.confirm = &confirm_overwrite,
		};
		ioe_errlst_init(&args.result.errors);

		confirm_called = 0;
		assert_int_equal(IO_RES_FAILED, ior_mv(&args));
		assert_int_equal(0, confirm_called);

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	delete_file(SANDBOX_PATH "/empty");
}

TEST(confirm_is_called_for_file_overwrite)
{
	create_empty_file(SANDBOX_PATH "/empty");
	clone_file(TEST_DATA_PATH "/read/two-lines", SANDBOX_PATH "/two-lines");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/two-lines",
			.arg2.dst = SANDBOX_PATH "/empty",
			.arg3.crs = IO_CRS_REPLACE_FILES,

			.confirm = &confirm_overwrite,
		};
		ioe_errlst_init(&args.result.errors);

		confirm_called = 0;
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
		assert_int_equal(1, confirm_called);

		assert_int_equal(0, args.result.errors.error_count);
	}

	delete_file(SANDBOX_PATH "/empty");
}

TEST(confirm_is_called_for_dir_overwrite, IF(regular_unix_user))
{
	create_empty_dir(SANDBOX_PATH "/src");
	create_empty_file(SANDBOX_PATH "/src/file");
	create_empty_file(SANDBOX_PATH "/file");

	assert_success(chmod(SANDBOX_PATH "/src", 0500));

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/src/file",
			.arg2.dst = SANDBOX_PATH "/file",
			.arg3.crs = IO_CRS_REPLACE_FILES,

			.confirm = &confirm_overwrite,
		};
		ioe_errlst_init(&args.result.errors);

		confirm_called = 0;
		assert_int_equal(IO_RES_FAILED, ior_mv(&args));
		assert_int_equal(1, confirm_called);

		assert_int_equal(1, args.result.errors.error_count);
		ioe_errlst_free(&args.result.errors);
	}

	assert_success(chmod(SANDBOX_PATH "/src", 0700));
	delete_tree(SANDBOX_PATH "/src");
	delete_file(SANDBOX_PATH "/file");
}

TEST(deny_to_overwrite_is_considered)
{
	create_empty_file(SANDBOX_PATH "/empty");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read/two-lines",
			.arg2.dst = SANDBOX_PATH "/empty",
			.arg3.crs = IO_CRS_REPLACE_FILES,

			.confirm = &deny_overwrite,
		};
		ioe_errlst_init(&args.result.errors);

		confirm_called = 0;
		assert_int_equal(IO_RES_SUCCEEDED, ior_mv(&args));
		assert_int_equal(1, confirm_called);

		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_true(file_exists(TEST_DATA_PATH "/read/two-lines"));

	delete_file(SANDBOX_PATH "/empty");
}

static int
confirm_overwrite(io_args_t *args, const char src[], const char dst[])
{
	++confirm_called;
	return 1;
}

static int
deny_overwrite(io_args_t *args, const char src[], const char dst[])
{
	++confirm_called;
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
