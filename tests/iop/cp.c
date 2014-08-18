#include "seatest.h"

#include <stdio.h> /* EOF FILE fclose() fopen() fread() */

#include "../../src/io/iop.h"

static int
files_are_identical(const char a[], const char b[])
{
	FILE *const a_file = fopen(a, "rb");
	FILE *const b_file = fopen(b, "rb");
	int a_data, b_data;

	do
	{
		a_data = fgetc(a_file);
		b_data = fgetc(b_file);
	}
	while(a_data != EOF && b_data != EOF);

	fclose(b_file);
	fclose(a_file);

	return a_data == b_data && a_data == EOF;
}

static void
test_dir_is_not_copied(void)
{
	io_args_t args =
	{
		.arg1.src = "../existing-files",
		.arg2.dst = "existing-files",
	};
	assert_false(iop_cp(&args) == 0);
}

static void
test_file_is_not_overwritten_if_not_asked(void)
{
	{
		io_args_t args =
		{
			.arg1.path = "empty",
		};
		assert_int_equal(0, iop_mkfile(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "empty",
			.arg2.dst = "empty-copy",
		};
		assert_int_equal(0, iop_cp(&args));
	}

	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "empty-copy",
			.arg3.overwrite = 0,
		};
		assert_false(iop_cp(&args) == 0);
	}

	assert_true(files_are_identical("empty", "empty-copy"));
	assert_false(files_are_identical("../read/two-lines", "empty-copy"));

	{
		io_args_t args =
		{
			.arg1.path = "empty",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	{
		io_args_t args =
		{
			.arg1.path = "empty-copy",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

static void
test_file_is_overwritten_if_asked(void)
{
	{
		io_args_t args =
		{
			.arg1.src = "../read/two-lines",
			.arg2.dst = "two-lines",
			.arg3.overwrite = 0,
		};
		assert_int_equal(0, iop_cp(&args));
	}

	assert_false(files_are_identical("../read/binary-data", "two-lines"));

	{
		io_args_t args =
		{
			.arg1.src = "../read/binary-data",
			.arg2.dst = "two-lines",
			.arg3.overwrite = 1,
		};
		assert_int_equal(0, iop_cp(&args));
	}

	assert_true(files_are_identical("../read/binary-data", "two-lines"));

	{
		io_args_t args =
		{
			.arg1.path = "two-lines",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}
}

static void
test_empty_file_is_copied(void)
{
}

static void
test_block_size_file_is_copied(void)
{
}

static void
test_block_size_minus_one_file_is_copied(void)
{
}

static void
test_block_size_plus_one_file_is_copied(void)
{
}

static void
test_double_block_size_file_is_copied(void)
{
}

static void
test_double_block_size_minus_one_file_is_copied(void)
{
}

static void
test_double_block_size_plus_one_file_is_copied(void)
{
}

void
cp_tests(void)
{
	test_fixture_start();

	run_test(test_dir_is_not_copied);
	run_test(test_file_is_not_overwritten_if_not_asked);
	run_test(test_file_is_overwritten_if_asked);
	run_test(test_empty_file_is_copied);
	run_test(test_block_size_file_is_copied);
	run_test(test_block_size_minus_one_file_is_copied);
	run_test(test_block_size_plus_one_file_is_copied);
	run_test(test_double_block_size_file_is_copied);
	run_test(test_double_block_size_minus_one_file_is_copied);
	run_test(test_double_block_size_plus_one_file_is_copied);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
