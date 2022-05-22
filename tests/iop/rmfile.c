#include <stic.h>

#include <unistd.h> /* F_OK access() rmdir() */

#include <test-utils.h>

#include "../../src/compat/os.h"
#include "../../src/io/iop.h"
#include "../../src/utils/fs.h"

#include "utils.h"

#define DIRECTORY_NAME SANDBOX_PATH "/directory-to-remove"
#define FILE_NAME SANDBOX_PATH "/file-to-remove"

TEST(file_is_removed)
{
	create_test_file(FILE_NAME);

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_failure(access(FILE_NAME, F_OK));
}

TEST(directory_is_not_removed)
{
	os_mkdir(DIRECTORY_NAME, 0700);
	assert_true(is_dir(DIRECTORY_NAME));

	{
		io_args_t args = {
			.arg1.path = DIRECTORY_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, iop_rmfile(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	assert_true(is_dir(DIRECTORY_NAME));

	rmdir(DIRECTORY_NAME);
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(symlink_is_removed_but_not_its_target, IF(not_windows))
{
	create_test_file(FILE_NAME);

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
			.arg2.target = "link",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_ln(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access("link", F_OK));

	{
		io_args_t args = {
			.arg1.path = "link",
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_failure(access("link", F_OK));

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_failure(access(FILE_NAME, F_OK));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
