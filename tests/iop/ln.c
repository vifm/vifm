#include <stic.h>

#include <unistd.h> /* F_OK access() */

#include <stdio.h> /* remove() */

#include <test-utils.h>

#include "../../src/io/iop.h"

#include "utils.h"

static void new_file(const char path[]);

static const char *const ORIG_FILE_NAME = "file";
static const char *const ORIG_FILE_PATH = SANDBOX_PATH "/file";
static const char *const NEW_ORIG_FILE_NAME = SANDBOX_PATH "/new_file";
static const char *const LINK_NAME = SANDBOX_PATH "/link";

SETUP()
{
	new_file(ORIG_FILE_PATH);
}

TEARDOWN()
{
	assert_success(access(ORIG_FILE_PATH, F_OK));
	assert_success(remove(ORIG_FILE_PATH));
	assert_failure(access(ORIG_FILE_PATH, F_OK));
}

TEST(existent_file_is_not_overwritten_if_not_requested)
{
	assert_failure(access(LINK_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = LINK_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_mkfile(&args));

		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access(LINK_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = ORIG_FILE_PATH,
			.arg2.target = LINK_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, iop_ln(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	assert_success(remove(LINK_NAME));

	assert_failure(access(LINK_NAME, F_OK));
}

TEST(existent_non_symlink_is_not_overwritten)
{
	new_file(LINK_NAME);

	{
		io_args_t args = {
			.arg1.path = ORIG_FILE_PATH,
			.arg2.target = LINK_NAME,
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, iop_ln(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	assert_success(remove(LINK_NAME));
	assert_failure(access(LINK_NAME, F_OK));
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(nonexistent_symlink_is_created, IF(not_windows))
{
	assert_failure(access(LINK_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = ORIG_FILE_NAME,
			.arg2.target = LINK_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_ln(&args));

		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access(LINK_NAME, F_OK));
	assert_success(remove(LINK_NAME));
	assert_failure(access(LINK_NAME, F_OK));
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(existent_symlink_is_changed, IF(not_windows))
{
	io_args_t args = {
		.arg1.path = ORIG_FILE_NAME,
		.arg2.target = LINK_NAME,
	};
	ioe_errlst_init(&args.result.errors);

	assert_failure(access(LINK_NAME, F_OK));

	assert_int_equal(IO_RES_SUCCEEDED, iop_ln(&args));
	assert_int_equal(0, args.result.errors.error_count);

	assert_success(access(LINK_NAME, F_OK));

	new_file(NEW_ORIG_FILE_NAME);

	args.arg1.path = NEW_ORIG_FILE_NAME;
	args.arg3.crs = IO_CRS_REPLACE_FILES;
	assert_int_equal(IO_RES_SUCCEEDED, iop_ln(&args));
	assert_int_equal(0, args.result.errors.error_count);

	assert_success(remove(LINK_NAME));
	assert_failure(access(LINK_NAME, F_OK));

	assert_success(remove(NEW_ORIG_FILE_NAME));
	assert_failure(access(NEW_ORIG_FILE_NAME, F_OK));
}

static void
new_file(const char path[])
{
	assert_failure(access(path, F_OK));

	{
		io_args_t args = {
			.arg1.path = path,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_mkfile(&args));

		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access(path, F_OK));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
