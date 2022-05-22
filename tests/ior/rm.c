#include <stic.h>

#include <unistd.h> /* F_OK access() */

#include "../../src/compat/os.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"

#include "utils.h"

#define DIRECTORY_NAME SANDBOX_PATH "/directory-to-remove"
#define FILE_NAME "file-to-remove"

TEST(file_is_removed)
{
	create_empty_file(SANDBOX_PATH "/" FILE_NAME);

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/" FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_failure(access(SANDBOX_PATH "/" FILE_NAME, F_OK));
}

TEST(empty_directory_is_removed)
{
	os_mkdir(DIRECTORY_NAME, 0700);
	assert_success(access(DIRECTORY_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.src = DIRECTORY_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_failure(access(DIRECTORY_NAME, F_OK));
}

TEST(non_empty_directory_is_removed)
{
	os_mkdir(DIRECTORY_NAME, 0700);
	assert_success(access(DIRECTORY_NAME, F_OK));
	create_empty_file(DIRECTORY_NAME "/" FILE_NAME);

	{
		io_args_t args = {
			.arg1.src = DIRECTORY_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_failure(access(DIRECTORY_NAME, F_OK));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
