#include <stic.h>

#include <unistd.h> /* F_OK access() */

#include <stdio.h> /* remove() */

#include <test-utils.h>

#include "../../src/io/ioeta.h"
#include "../../src/io/iop.h"
#include "../../src/io/ior.h"

static const char *const FILE_NAME = SANDBOX_PATH "/file-to-create";

TEST(file_is_created)
{
	assert_failure(access(FILE_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_mkfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access(FILE_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
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

		assert_int_equal(IO_RES_SUCCEEDED, iop_mkfile(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_success(access(FILE_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, iop_mkfile(&args));

		assert_true(args.result.errors.error_count != 0);
		ioe_errlst_free(&args.result.errors);
	}

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}
}

TEST(creating_files_reports_progress)
{
	const io_cancellation_t no_cancellation = {};

	io_args_t args = {
		.arg1.path = FILE_NAME,

		.estim = ioeta_alloc(NULL, no_cancellation),
	};
	ioe_errlst_init(&args.result.errors);

	assert_int_equal(IO_RES_SUCCEEDED, iop_mkfile(&args));
	assert_int_equal(0, args.result.errors.error_count);

	assert_success(remove(FILE_NAME));
	assert_int_equal(1, args.estim->current_item);
	assert_true(args.estim->item != NULL);

	ioeta_free(args.estim);
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(broken_link_is_an_existing_file, IF(not_windows))
{
	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/no-such-file",
			.arg2.target = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_SUCCEEDED, iop_ln(&args));

		assert_int_equal(0, args.result.errors.error_count);
	}

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_int_equal(IO_RES_FAILED, iop_mkfile(&args));
		assert_int_equal(1, args.result.errors.error_count);
		assert_string_equal("Such file already exists",
				args.result.errors.errors[0].msg);
		ioe_errlst_free(&args.result.errors);
	}

	assert_success(remove(FILE_NAME));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
