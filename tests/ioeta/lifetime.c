#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/io/ioeta.h"

TEST(free_of_null_is_ok)
{
	ioeta_free(NULL);
}

TEST(alloc_initializes_all_fields)
{
	const io_cancellation_t no_cancellation = {};
	ioeta_estim_t *const estim = ioeta_alloc(NULL, no_cancellation);

	assert_int_equal(0, estim->total_items);
	assert_int_equal(0, estim->current_item);
	assert_int_equal(0, estim->total_bytes);
	assert_int_equal(0, estim->current_byte);
	assert_string_equal(NULL, estim->item);

	ioeta_free(estim);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
