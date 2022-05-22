#include <stic.h>

#include <unistd.h> /* F_OK access() */

#include "../../src/compat/os.h"
#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"

#include "utils.h"

#define DIRECTORY_NAME SANDBOX_PATH "/directory-to-remove"
#define FILE_NAME DIRECTORY_NAME "/file-to-remove"

TEST(empty_directory_is_removed)
{
	os_mkdir(DIRECTORY_NAME, 0700);
	assert_true(is_dir(DIRECTORY_NAME));

	{
		io_args_t args = {
			.arg1.path = DIRECTORY_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_rmdir(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_failure(access(DIRECTORY_NAME, F_OK));
}

TEST(non_empty_directory_is_not_removed)
{
	os_mkdir(DIRECTORY_NAME, 0700);
	assert_true(is_dir(DIRECTORY_NAME));
	create_test_file(FILE_NAME);

	{
		io_args_t args = {
			.arg1.path = DIRECTORY_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, iop_rmdir(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	assert_true(is_dir(DIRECTORY_NAME));

	{
		io_args_t args = {
			.arg1.path = DIRECTORY_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_failure(access(DIRECTORY_NAME, F_OK));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
