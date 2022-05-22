#include <stic.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/io/iop.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/utils.h"

#include "utils.h"

static int confirm_overwrite(io_args_t *args, const char src[],
		const char dst[]);
static int deny_overwrite(io_args_t *args, const char src[], const char dst[]);

static int confirm_called;

TEST(confirm_is_not_called_for_no_overwrite)
{
	create_test_file(SANDBOX_PATH "/empty");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read/two-lines",
			.arg2.dst = SANDBOX_PATH "/empty",
			.arg3.crs = IO_CRS_FAIL,

			.confirm = &confirm_overwrite,
		};
		ioe_errlst_init(&args.result.errors);

		confirm_called = 0;
		assert_int_equal(IO_RES_FAILED, iop_cp(&args));
		assert_int_equal(0, confirm_called);

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	delete_test_file(SANDBOX_PATH "/empty");
}

TEST(confirm_is_called_for_overwrite)
{
	create_test_file(SANDBOX_PATH "/empty");
	clone_test_file(SANDBOX_PATH "/empty", SANDBOX_PATH "/empty-copy");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read/two-lines",
			.arg2.dst = SANDBOX_PATH "/empty",
			.arg3.crs = IO_CRS_REPLACE_FILES,

			.confirm = &confirm_overwrite,
		};
		ioe_errlst_init(&args.result.errors);

		confirm_called = 0;
		assert_int_equal(IO_RES_SUCCEEDED, iop_cp(&args));
		assert_int_equal(1, confirm_called);

		/* Skipping file by user is not real error. */
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_true(files_are_identical(TEST_DATA_PATH "/read/two-lines",
				SANDBOX_PATH "/empty"));
	assert_false(files_are_identical(SANDBOX_PATH "/empty",
				SANDBOX_PATH "/empty-copy"));

	delete_test_file(SANDBOX_PATH "/empty");
	delete_test_file(SANDBOX_PATH "/empty-copy");
}

TEST(no_confirm_on_appending)
{
	create_test_file(SANDBOX_PATH "/empty");
	clone_test_file(SANDBOX_PATH "/empty", SANDBOX_PATH "/empty-copy");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read/two-lines",
			.arg2.dst = SANDBOX_PATH "/empty",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,

			.confirm = &confirm_overwrite,
		};
		ioe_errlst_init(&args.result.errors);

		confirm_called = 0;
		assert_int_equal(IO_RES_SUCCEEDED, iop_cp(&args));
		assert_int_equal(0, confirm_called);

		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_true(files_are_identical(TEST_DATA_PATH "/read/two-lines",
				SANDBOX_PATH "/empty"));
	assert_false(files_are_identical(SANDBOX_PATH "/empty",
				SANDBOX_PATH "/empty-copy"));

	delete_test_file(SANDBOX_PATH "/empty");
	delete_test_file(SANDBOX_PATH "/empty-copy");
}

TEST(deny_to_overwrite_is_considered)
{
	create_test_file(SANDBOX_PATH "/empty");
	clone_test_file(SANDBOX_PATH "/empty", SANDBOX_PATH "/empty-copy");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read/two-lines",
			.arg2.dst = SANDBOX_PATH "/empty",
			.arg3.crs = IO_CRS_REPLACE_FILES,

			.confirm = &deny_overwrite,
		};
		ioe_errlst_init(&args.result.errors);

		confirm_called = 0;
		assert_int_equal(IO_RES_SUCCEEDED, iop_cp(&args));
		assert_int_equal(1, confirm_called);

		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_false(files_are_identical(TEST_DATA_PATH "/read/two-lines",
				SANDBOX_PATH "/empty"));
	assert_true(files_are_identical(SANDBOX_PATH "/empty",
				SANDBOX_PATH "/empty-copy"));

	delete_test_file(SANDBOX_PATH "/empty");
	delete_test_file(SANDBOX_PATH "/empty-copy");
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
