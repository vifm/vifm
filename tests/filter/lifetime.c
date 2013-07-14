#include "seatest.h"

#include "../../src/utils/filter.h"

static void
test_filter_created_empty(void)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 0));
	assert_true(filter_is_empty(&filter));
	filter_dispose(&filter);
}

static void
test_case_insensitivity(void)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 0));
	assert_int_equal(0, filter_set(&filter, "abcd"));
	assert_true(filter_matches(&filter, "aBCd"));
	filter_dispose(&filter);
}

static void
test_case_sensitivity(void)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));
	assert_int_equal(0, filter_set(&filter, "abcd"));
	assert_true(filter_matches(&filter, "abcd"));
	assert_false(filter_matches(&filter, "aBCd"));
	filter_dispose(&filter);
}

void
lifetime_tests(void)
{
	test_fixture_start();

	run_test(test_filter_created_empty);
	run_test(test_case_insensitivity);
	run_test(test_case_sensitivity);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
