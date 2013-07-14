#include "seatest.h"

#include "../../src/utils/filter.h"

static void
test_filter_matches_differently_after_set(void)
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

static void
test_set_to_empty_is_like_clear(void)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_int_equal(0, filter_set(&filter, ""));
	assert_true(filter_is_empty(&filter));

	filter_dispose(&filter);
}

void
set_tests(void)
{
	test_fixture_start();

	run_test(test_filter_matches_differently_after_set);
	run_test(test_set_to_empty_is_like_clear);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
