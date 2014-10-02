#include "seatest.h"

#include <stddef.h> /* NULL */

#include "../../src/io/private/ioeta.h"
#include "../../src/io/ioeta.h"

static ioeta_estim_t *estim;

static void
setup(void)
{
	estim = ioeta_alloc(NULL);
}

static void
teardown(void)
{
	ioeta_free(estim);
	estim = NULL;
}

static void
test_add_item_increments_number_of_items(void)
{
	const int prev = estim->total_items;
	ioeta_add_item(estim, "path");
	assert_int_equal(prev + 1, estim->total_items);
}

static void
test_add_file_increments_number_of_items(void)
{
	const int prev = estim->total_items;
	ioeta_add_file(estim, "path");
	assert_int_equal(prev + 1, estim->total_items);
}

static void
test_add_dir_increments_number_of_bytes(void)
{
	const int prev = estim->total_bytes;
	ioeta_add_file(estim, "test-data/read/binary-data");
	assert_int_equal(prev + 1024, estim->total_bytes);
}

static void
test_add_dir_does_not_increment_number_of_items(void)
{
	const int prev = estim->total_items;
	ioeta_add_dir(estim, "path");
	assert_int_equal(prev, estim->total_items);
}

static void
test_add_dir_does_not_increment_number_of_bytes(void)
{
	const int prev = estim->total_bytes;
	ioeta_add_dir(estim, "test-data");
	assert_int_equal(prev, estim->total_bytes);
}

static void
test_update_increments_current_byte(void)
{
	const int prev = estim->current_byte;
	ioeta_update(estim, "a", 0, 134);
	assert_int_equal(prev + 134, estim->current_byte);
}

static void
test_update_sets_current_file(void)
{
	assert_string_equal(NULL, estim->item);
	ioeta_update(estim, "a", 0, 134);
	assert_string_equal("a", estim->item);
	ioeta_update(estim, "b", 0, 134);
	assert_string_equal("b", estim->item);
}

static void
test_update_with_null_file_does_not_set_current_file(void)
{
	assert_string_equal(NULL, estim->item);
	ioeta_update(estim, "a", 0, 134);
	assert_string_equal("a", estim->item);
	ioeta_update(estim, NULL, 0, 134);
	assert_string_equal("a", estim->item);
}

static void
test_zero_update_changes_nothing(void)
{
	ioeta_estim_t e = *estim;
	ioeta_update(estim, "b", 0, 0);
	assert_int_equal(e.current_item, estim->current_item);
}

static void
test_finished_update_increments_current_item(void)
{
	const int prev = estim->current_item;
	ioeta_update(estim, "c", 1, 0);
	assert_int_equal(prev + 1, estim->current_item);
}

void
update_tests(void)
{
	test_fixture_start();

	fixture_setup(&setup);
	fixture_teardown(&teardown);

	run_test(test_add_item_increments_number_of_items);
	run_test(test_add_file_increments_number_of_items);
	run_test(test_add_dir_increments_number_of_bytes);
	run_test(test_add_dir_does_not_increment_number_of_items);
	run_test(test_add_dir_does_not_increment_number_of_bytes);
	run_test(test_update_increments_current_byte);
	run_test(test_update_sets_current_file);
	run_test(test_update_with_null_file_does_not_set_current_file);
	run_test(test_zero_update_changes_nothing);
	run_test(test_finished_update_increments_current_item);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
