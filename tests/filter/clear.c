#include <stic.h>

#include "../../src/utils/filter.h"

TEST(filter_empty_after_clear)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_int_equal(0, filter_set(&filter, "abc"));
	assert_false(filter_is_empty(&filter));

	filter_clear(&filter);
	assert_true(filter_is_empty(&filter));

	filter_dispose(&filter);
}

TEST(double_clear_is_ok)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_int_equal(0, filter_set(&filter, "abc"));
	assert_false(filter_is_empty(&filter));

	filter_clear(&filter);
	assert_true(filter_is_empty(&filter));

	filter_clear(&filter);
	assert_true(filter_is_empty(&filter));

	filter_dispose(&filter);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
