#include "seatest.h"

#include <unistd.h> /* F_OK access() getcwd() */

#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/fs_limits.h"

static void create_directory(const char path[], const char root[],
		int create_parents);

#define DIR_NAME "dir-to-create"
#define NESTED_DIR_NAME "dir-to-create/dir-to-create"

static void
test_single_dir_is_created(void)
{
	create_directory(DIR_NAME, DIR_NAME, 0);
}

static void
test_child_dir_and_parent_are_created(void)
{
	create_directory(NESTED_DIR_NAME, DIR_NAME, 1);
}

static void
test_create_by_absolute_path(void)
{
	char cwd[PATH_MAX];
	char full_path[PATH_MAX];

	getcwd(cwd, sizeof(cwd));
	snprintf(full_path, sizeof(full_path), "%s/%s", cwd, NESTED_DIR_NAME);
	create_directory(full_path, DIR_NAME, 1);
}

static void
create_directory(const char path[], const char root[], int create_parents)
{
	assert_int_equal(-1, access(path, F_OK));

	{
		io_args_t args =
		{
			.arg1.path = path,
			.arg2.process_parents = create_parents,
			.arg3.mode = 0700,
		};
		assert_int_equal(0, iop_mkdir(&args));
	}

	assert_int_equal(0, access(path, F_OK));
	assert_true(is_dir(path));

	{
		io_args_t args =
		{
			.arg1.path = root,
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_child_dir_is_not_created(void)
{
	assert_int_equal(-1, access(NESTED_DIR_NAME, F_OK));

	{
		io_args_t args =
		{
			.arg1.path = NESTED_DIR_NAME,
			.arg2.process_parents = 0,
		};
		assert_false(iop_mkdir(&args) == 0);
	}

	assert_int_equal(-1, access(NESTED_DIR_NAME, F_OK));
}

#if !defined(WIN32) && !defined(__CYGWIN__)

static void
test_permissions_are_taken_into_account(void)
{
	{
		io_args_t args =
		{
			.arg1.path = DIR_NAME,
			.arg2.process_parents = 0,
			.arg3.mode = 0000,
		};
		assert_int_equal(0, iop_mkdir(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = NESTED_DIR_NAME,
			.arg2.process_parents = 0,
		};
		assert_false(iop_mkdir(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = DIR_NAME,
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

static void
test_permissions_are_taken_into_account_for_the_most_nested_only(void)
{
	{
		io_args_t args =
		{
			.arg1.path = NESTED_DIR_NAME,
			.arg2.process_parents = 1,
			.arg3.mode = 0000,
		};
		assert_int_equal(0, iop_mkdir(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = DIR_NAME "/dir",
			.arg2.process_parents = 0,
			.arg3.mode = 0755,
		};
		assert_int_equal(0, iop_mkdir(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = NESTED_DIR_NAME "/dir",
			.arg2.process_parents = 0,
			.arg3.mode = 0755,
		};
		assert_false(iop_mkdir(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = NESTED_DIR_NAME,
		};
		assert_int_equal(0, iop_rmdir(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = DIR_NAME "/dir",
		};
		assert_int_equal(0, iop_rmdir(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = DIR_NAME,
		};
		assert_int_equal(0, iop_rmdir(&args));
	}
}

#endif

void
mkdir_tests(void)
{
	test_fixture_start();

	run_test(test_single_dir_is_created);
	run_test(test_child_dir_and_parent_are_created);
	run_test(test_create_by_absolute_path);
	run_test(test_child_dir_is_not_created);
#if !defined(WIN32) && !defined(__CYGWIN__)
	run_test(test_permissions_are_taken_into_account);
	run_test(test_permissions_are_taken_into_account_for_the_most_nested_only);
#endif

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
