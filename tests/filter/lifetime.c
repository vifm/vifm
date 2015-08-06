#include <stic.h>

#include "../../src/utils/filter.h"

TEST(filter_created_empty)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 0));
	assert_true(filter_is_empty(&filter));
	filter_dispose(&filter);
}

TEST(case_insensitivity)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 0));
	assert_int_equal(0, filter_set(&filter, "abcd"));
	assert_true(filter_matches(&filter, "aBCd"));
	filter_dispose(&filter);
}

TEST(case_sensitivity)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));
	assert_int_equal(0, filter_set(&filter, "abcd"));
	assert_true(filter_matches(&filter, "abcd"));
	assert_false(filter_matches(&filter, "aBCd"));
	filter_dispose(&filter);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
