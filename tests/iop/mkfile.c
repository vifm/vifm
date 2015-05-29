#include <stic.h>

#include <unistd.h> /* F_OK access() */

#include "../../src/io/iop.h"
#include "../../src/io/ior.h"

static const char *const FILE_NAME = "file-to-create";

TEST(file_is_created)
{
	assert_failure(access(FILE_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_mkfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access(FILE_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(fails_if_file_exists)
{
	assert_failure(access(FILE_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(iop_mkfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access(FILE_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_failure(iop_mkfile(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
