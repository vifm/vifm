#include "seatest.h"

#include <stdio.h> /* FILE fopen() fclose() */

#include <unistd.h> /* F_OK access() rmdir() */

#include "../../src/io/iop.h"
#include "../../src/utils/fs.h"

static const char *const FILE_NAME = "file-to-remove";
static const char *const DIRECTORY_NAME = "directory-to-remove";

static void
test_file_is_removed(void)
{
	FILE *const f = fopen(FILE_NAME, "w");
	fclose(f);
	assert_int_equal(0, access(FILE_NAME, F_OK));

	{
		io_args_t args =
		{
			.arg1.path = FILE_NAME,
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	assert_int_equal(-1, access(FILE_NAME, F_OK));
}

static void
test_directory_is_not_removed(void)
{
	make_dir(DIRECTORY_NAME, 0700);
	assert_true(is_dir(DIRECTORY_NAME));

	{
		io_args_t args =
		{
			.arg1.path = DIRECTORY_NAME,
		};
		assert_false(iop_rmfile(&args) == 0);
	}

	assert_true(is_dir(DIRECTORY_NAME));

	rmdir(DIRECTORY_NAME);
}

#ifndef _WIN32

static void
test_symlink_is_removed_but_not_its_target(void)
{
	FILE *const f = fopen(FILE_NAME, "w");
	fclose(f);
	assert_int_equal(0, access(FILE_NAME, F_OK));

	{
		io_args_t args =
		{
			.arg1.path = FILE_NAME,
			.arg2.target = "link",
		};
		assert_int_equal(0, iop_ln(&args));
	}

	assert_int_equal(0, access("link", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = "link",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	assert_int_equal(-1, access("link", F_OK));

	{
		io_args_t args =
		{
			.arg1.path = FILE_NAME,
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	assert_int_equal(-1, access(FILE_NAME, F_OK));
}

#endif

void
rmfile_tests(void)
{
	test_fixture_start();

	run_test(test_file_is_removed);
	run_test(test_directory_is_not_removed);

#ifndef _WIN32
	/* Creating symbolic links on Windows requires administrator rights. */
	run_test(test_symlink_is_removed_but_not_its_target);
#endif

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
