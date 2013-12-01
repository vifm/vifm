#include "seatest.h"

#include <unistd.h> /* F_OK access() */

#include "../../src/io/iop.h"

static const char *const FILE_NAME = "file-to-create";

static void
setup(void)
{
	assert_int_equal(0, chdir("test-data/sandbox"));
}

static void
teardown(void)
{
	assert_int_equal(0, chdir("../.."));
}

static void
test_file_is_created(void)
{
	assert_int_equal(-1, access(FILE_NAME, F_OK));

	{
		io_args_t args =
		{
			.arg1.path = FILE_NAME,
		};
		assert_int_equal(0, iop_mkfile(&args));
	}

	assert_int_equal(0, access(FILE_NAME, F_OK));

	{
		io_args_t args =
		{
			.arg1.path = FILE_NAME,
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

static void
test_fails_if_file_exists(void)
{
	assert_int_equal(-1, access(FILE_NAME, F_OK));

	{
		io_args_t args =
		{
			.arg1.path = FILE_NAME,
		};
		assert_int_equal(0, iop_mkfile(&args));
	}

	assert_int_equal(0, access(FILE_NAME, F_OK));

	{
		io_args_t args =
		{
			.arg1.path = FILE_NAME,
		};
		assert_false(iop_mkfile(&args) == 0);
	}

	{
		io_args_t args =
		{
			.arg1.path = FILE_NAME,
		};
		assert_int_equal(0, ior_rm(&args));
	}
}

void
mkfile_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_file_is_created);
	/* Currently there is no check that file exists on *nix, so test will fail. */
#ifdef _WIN32
	run_test(test_fails_if_file_exists);
#endif

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
