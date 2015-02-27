#include <stic.h>

#include <stdio.h> /* FILE fopen() fclose() */

#include <unistd.h> /* F_OK access() */

#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"

static const char *const FILE_NAME = "file-to-remove";
static const char *const DIRECTORY_NAME = "directory-to-remove";

TEST(file_is_removed)
{
	{
		FILE *const f = fopen(FILE_NAME, "w");
		fclose(f);
		assert_int_equal(0, access(FILE_NAME, F_OK));
	}

	{
		io_args_t args =
		{
			.arg1.src = FILE_NAME,
		};
		assert_int_equal(0, ior_rm(&args));
	}

	assert_int_equal(-1, access(FILE_NAME, F_OK));
}

TEST(empty_directory_is_removed)
{
	make_dir(DIRECTORY_NAME, 0700);
	assert_int_equal(0, access(DIRECTORY_NAME, F_OK));

	{
		io_args_t args =
		{
			.arg1.src = DIRECTORY_NAME,
		};
		assert_int_equal(0, ior_rm(&args));
	}

	assert_int_equal(-1, access(DIRECTORY_NAME, F_OK));
}

TEST(non_empty_directory_is_removed)
{
	make_dir(DIRECTORY_NAME, 0700);
	assert_int_equal(0, access(DIRECTORY_NAME, F_OK));

	assert_int_equal(0, chdir(DIRECTORY_NAME));
	{
		FILE *const f = fopen(FILE_NAME, "w");
		fclose(f);
		assert_int_equal(0, access(FILE_NAME, F_OK));
	}
	assert_int_equal(0, chdir(".."));

	{
		io_args_t args =
		{
			.arg1.src = DIRECTORY_NAME,
		};
		assert_int_equal(0, ior_rm(&args));
	}

	assert_int_equal(-1, access(DIRECTORY_NAME, F_OK));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
