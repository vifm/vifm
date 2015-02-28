#include <stic.h>

#include <stdio.h> /* FILE fopen() fclose() */

#include <unistd.h> /* F_OK access() rmdir() */

#include "../../src/io/iop.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/utils.h"

static int not_windows(void);

static const char *const FILE_NAME = "file-to-remove";
static const char *const DIRECTORY_NAME = "directory-to-remove";

TEST(file_is_removed)
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

TEST(directory_is_not_removed)
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

/* Creating symbolic links on Windows requires administrator rights. */
TEST(symlink_is_removed_but_not_its_target, IF(not_windows))
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

static int
not_windows(void)
{
	return get_env_type() != ET_WIN;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
