#include "seatest.h"

#include <stddef.h> /* NULL */

#include "../../src/io/ioeta.h"

static void
test_free_of_null_is_ok(void)
{
	ioeta_free(NULL);
}

static void
test_alloc_initializes_all_fields(void)
{
	ioeta_estim_t *const estim = ioeta_alloc(NULL);

	assert_int_equal(0, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(0, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);
	assert_string_equal(NULL, estim->item);

	ioeta_free(estim);
}

void
lifetime_tests(void)
{
	test_fixture_start();

	run_test(test_free_of_null_is_ok);
	run_test(test_alloc_initializes_all_fields);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
