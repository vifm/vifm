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
		assert_success(iop_mkfile(&args));
	}

	assert_success(access(FILE_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		assert_success(ior_rm(&args));
	}
}

TEST(fails_if_file_exists)
{
	assert_failure(access(FILE_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		assert_success(iop_mkfile(&args));
	}

	assert_success(access(FILE_NAME, F_OK));

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		assert_failure(iop_mkfile(&args));
	}

	{
		io_args_t args = {
			.arg1.path = FILE_NAME,
		};
		assert_success(ior_rm(&args));
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
