#include "seatest.h"

#include <stddef.h> /* NULL */

#include "../../src/io/private/ioeta.h"
#include "../../src/io/ioeta.h"

static void
test_empty_files_are_ok(void)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL);

	ioeta_calculate(estim, "test-data/existing-files");

	assert_int_equal(4, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(0, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);

	ioeta_free(estim);
}

static void
test_non_empty_files_are_ok(void)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL);

	ioeta_calculate(estim, "test-data/various-sizes");

	assert_int_equal(8, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(73728, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);

	ioeta_free(estim);
}

void
calculate_tests(void)
{
	test_fixture_start();

	run_test(test_empty_files_are_ok);
	run_test(test_non_empty_files_are_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
