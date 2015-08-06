#include <stic.h>

#include "../../src/utils/filter.h"

TEST(returns_positive_number_on_match)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_int_equal(0, filter_set(&filter, "abcd"));
	assert_true(filter_matches(&filter, "abcd") > 0);

	filter_dispose(&filter);
}

TEST(returns_zero_on_no_match)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_int_equal(0, filter_set(&filter, "abcd"));
	assert_true(filter_matches(&filter, "123") == 0);

	filter_dispose(&filter);
}

TEST(returns_negative_number_for_empty_filter)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_true(filter_matches(&filter, "123") < 0);

	filter_dispose(&filter);
}

TEST(returns_negative_number_for_invalid_regexp)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_false(filter_set(&filter, "*") == 0);
	assert_true(filter_matches(&filter, "123") < 0);

	filter_dispose(&filter);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
