#include <stic.h>

#include "../../src/utils/filter.h"

TEST(filter_matches_differently_after_set)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_int_equal(0, filter_set(&filter, "abcd"));
	assert_true(filter_matches(&filter, "abcd"));

	assert_int_equal(0, filter_set(&filter, "a123"));
	assert_false(filter_matches(&filter, "abcd"));
	assert_true(filter_matches(&filter, "a123"));

	filter_dispose(&filter);
}

TEST(set_to_empty_is_like_clear)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_int_equal(0, filter_set(&filter, ""));
	assert_true(filter_is_empty(&filter));

	filter_dispose(&filter);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
