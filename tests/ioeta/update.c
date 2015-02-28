#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/io/private/ioeta.h"
#include "../../src/io/ioeta.h"

static ioeta_estim_t *estim;

SETUP()
{
	estim = ioeta_alloc(NULL);
}

TEARDOWN()
{
	ioeta_free(estim);
	estim = NULL;
}

TEST(add_item_increments_number_of_items)
{
	const int prev = estim->total_items;
	ioeta_add_item(estim, "path");
	assert_int_equal(prev + 1, estim->total_items);
}

TEST(add_file_increments_number_of_items)
{
	const int prev = estim->total_items;
	ioeta_add_file(estim, "path");
	assert_int_equal(prev + 1, estim->total_items);
}

TEST(add_dir_increments_number_of_bytes)
{
	const int prev = estim->total_bytes;
	ioeta_add_file(estim, "test-data/read/binary-data");
	assert_int_equal(prev + 1024, estim->total_bytes);
}

TEST(add_dir_does_not_increment_number_of_items)
{
	const int prev = estim->total_items;
	ioeta_add_dir(estim, "path");
	assert_int_equal(prev, estim->total_items);
}

TEST(add_dir_does_not_increment_number_of_bytes)
{
	const int prev = estim->total_bytes;
	ioeta_add_dir(estim, "test-data");
	assert_int_equal(prev, estim->total_bytes);
}

TEST(update_increments_current_byte)
{
	const int prev = estim->current_byte;
	ioeta_update(estim, "a", 0, 134);
	assert_int_equal(prev + 134, estim->current_byte);
}

TEST(update_sets_current_file)
{
	assert_string_equal(NULL, estim->item);
	ioeta_update(estim, "a", 0, 134);
	assert_string_equal("a", estim->item);
	ioeta_update(estim, "b", 0, 134);
	assert_string_equal("b", estim->item);
}

TEST(update_with_null_file_does_not_set_current_file)
{
	assert_string_equal(NULL, estim->item);
	ioeta_update(estim, "a", 0, 134);
	assert_string_equal("a", estim->item);
	ioeta_update(estim, NULL, 0, 134);
	assert_string_equal("a", estim->item);
}

TEST(zero_update_changes_nothing)
{
	ioeta_estim_t e = *estim;
	ioeta_update(estim, "b", 0, 0);
	assert_int_equal(e.current_item, estim->current_item);
}

TEST(finished_update_increments_current_item)
{
	const int prev = estim->current_item;
	ioeta_update(estim, "c", 1, 0);
	assert_int_equal(prev + 1, estim->current_item);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
