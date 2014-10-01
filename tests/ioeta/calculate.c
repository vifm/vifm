#include "seatest.h"

#include <stddef.h> /* NULL */

#include "../../src/io/private/ioeta.h"
#include "../../src/io/ioeta.h"
#include "../../src/io/iop.h"


static void
test_non_existent_path_yields_zero_size(void)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL);

	ioeta_calculate(estim, "does-not-exist:;", 0);

	assert_int_equal(1, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(0, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);

	ioeta_free(estim);
}

static void
test_empty_files_are_ok(void)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL);

	ioeta_calculate(estim, "test-data/existing-files", 0);

	assert_int_equal(3, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(0, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);

	ioeta_free(estim);
}

static void
test_non_empty_files_are_ok(void)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL);

	ioeta_calculate(estim, "test-data/various-sizes", 0);

	assert_int_equal(7, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(73728, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);

	ioeta_free(estim);
}

static void
test_shallow_estimation_does_not_recur(void)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL);

	ioeta_calculate(estim, "test-data/various-sizes", 1);

	assert_int_equal(1, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(0, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);

	ioeta_free(estim);
}

#ifndef _WIN32

static void
test_symlink_calculated_as_zero_bytes(void)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL);

	{
		io_args_t args =
		{
			.arg1.path = "test-data/existing-files",
			.arg2.target = "link",
		};
		assert_int_equal(0, iop_ln(&args));
	}

	ioeta_calculate(estim, "link", 0);

	assert_int_equal(1, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(0, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);

	{
		io_args_t args =
		{
			.arg1.path = "link",
		};
		assert_int_equal(0, iop_rmfile(&args));
	}

	ioeta_free(estim);
}

#endif

void
calculate_tests(void)
{
	test_fixture_start();

	run_test(test_non_existent_path_yields_zero_size);
	run_test(test_empty_files_are_ok);
	run_test(test_non_empty_files_are_ok);
	run_test(test_shallow_estimation_does_not_recur);

#ifndef _WIN32
	/* Creating symbolic links on Windows requires administrator rights. */
	run_test(test_symlink_calculated_as_zero_bytes);
#endif

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
