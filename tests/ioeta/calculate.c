#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/io/private/ioeta.h"
#include "../../src/io/ioeta.h"
#include "../../src/io/iop.h"

static const io_cancellation_t no_cancellation;

TEST(non_existent_path_yields_zero_size)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL, no_cancellation);

	ioeta_calculate(estim, "does-not-exist:;", 0);

	assert_int_equal(1, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(0, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);

	ioeta_free(estim);
}

TEST(empty_files_are_ok)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL, no_cancellation);

	ioeta_calculate(estim, TEST_DATA_PATH "/existing-files", 0);

	assert_int_equal(3, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(0, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);

	ioeta_free(estim);
}

TEST(non_empty_files_are_ok)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL, no_cancellation);

	ioeta_calculate(estim, TEST_DATA_PATH "/various-sizes", 0);

	assert_int_equal(7, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(73728, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);

	ioeta_free(estim);
}

TEST(shallow_estimation_does_not_recur)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL, no_cancellation);

	ioeta_calculate(estim, TEST_DATA_PATH "/various-sizes", 1);

	assert_int_equal(1, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(0, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);

	ioeta_free(estim);
}

#ifndef _WIN32

TEST(symlink_calculated_as_zero_bytes)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL, no_cancellation);

	{
		io_args_t args = {
			.arg1.path = TEST_DATA_PATH "/existing-files",
			.arg2.target = SANDBOX_PATH "/link",
		};
		assert_int_equal(IO_RES_SUCCEEDED, iop_ln(&args));
	}

	ioeta_calculate(estim, SANDBOX_PATH "/link", 0);

	assert_int_equal(1, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(0, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/link",
		};
		assert_int_equal(IO_RES_SUCCEEDED, iop_rmfile(&args));
	}

	ioeta_free(estim);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
