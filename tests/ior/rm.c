#include <stic.h>

#include <stdio.h> /* FILE fopen() fclose() */

#include <unistd.h> /* F_OK access() */

#include "../../src/compat/os.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"

static const char *const FILE_NAME = "file-to-remove";
static const char *const DIRECTORY_NAME = "directory-to-remove";

TEST(file_is_removed)
{
	{
		FILE *const f = fopen(FILE_NAME, "w");
		fclose(f);
		assert_success(access(FILE_NAME, F_OK));
	}

	{
		io_args_t args = {
			.arg1.src = FILE_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_failure(access(FILE_NAME, F_OK));
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

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_failure(access(DIRECTORY_NAME, F_OK));
}

TEST(non_empty_directory_is_removed)
{
	os_mkdir(DIRECTORY_NAME, 0700);
	assert_success(access(DIRECTORY_NAME, F_OK));

	assert_success(chdir(DIRECTORY_NAME));
	{
		FILE *const f = fopen(FILE_NAME, "w");
		fclose(f);
		assert_success(access(FILE_NAME, F_OK));
	}
	assert_success(chdir(".."));

	{
		io_args_t args = {
			.arg1.src = DIRECTORY_NAME,
		};
		ioe_errlst_init(&args.result.errors);

		assert_success(ior_rm(&args));
		assert_int_equal(0, args.result.errors.error_count);
	}

	assert_failure(access(DIRECTORY_NAME, F_OK));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
