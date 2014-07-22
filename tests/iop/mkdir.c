#include "seatest.h"

#include <unistd.h> /* F_OK access() */

#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/fs.h"

static void create_directory(const char path[], const char root[],
		int create_parents);

static const char *const DIR_NAME = "dir-to-create";
static const char *const NESTED_DIR_NAME = "dir-to-create/dir-to-create";

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
create_directory(const char path[], const char root[], int create_parents)
{
	assert_int_equal(-1, access(path, F_OK));

	{
		io_args_t args =
		{
			.arg1.path = path,
			.arg3.process_parents = create_parents,
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
			.arg3.process_parents = 0,
		};
		assert_false(iop_mkdir(&args) == 0);
	}

	assert_int_equal(-1, access(NESTED_DIR_NAME, F_OK));
}

void
mkdir_tests(void)
{
	test_fixture_start();

	run_test(test_single_dir_is_created);
	run_test(test_child_dir_and_parent_are_created);
	run_test(test_child_dir_is_not_created);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
