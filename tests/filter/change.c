#include <stic.h>

#include "../../src/utils/filter.h"

TEST(regexp_is_changed)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_int_equal(0, filter_set(&filter, "abcd"));
	assert_true(filter_matches(&filter, "abcd"));

	assert_int_equal(0, filter_change(&filter, "a123", 0));
	assert_false(filter_matches(&filter, "abcd"));
	assert_true(filter_matches(&filter, "a123"));

	filter_dispose(&filter);
}

TEST(case_sensitivity_is_changed)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_int_equal(0, filter_set(&filter, "abcd"));
	assert_true(filter_matches(&filter, "abcd"));
	assert_false(filter_matches(&filter, "aBCd"));

	assert_int_equal(0, filter_change(&filter, "abcd", 0));
	assert_true(filter_matches(&filter, "abcd"));
	assert_true(filter_matches(&filter, "aBCd"));

	filter_dispose(&filter);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
