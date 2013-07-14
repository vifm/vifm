#include "seatest.h"

#include "../../src/utils/filter.h"

static void
test_filter_empty_after_clear(void)
{
	filter_t filter;
	assert_int_equal(0, filter_init(&filter, 1));

	assert_int_equal(0, filter_set(&filter, "abc"));
	assert_false(filter_is_empty(&filter));

	filter_clear(&filter);
	assert_true(filter_is_empty(&filter));

	filter_dispose(&filter);
}

static void
test_double_clear_is_ok(void)
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

void
clear_tests(void)
{
	test_fixture_start();

	run_test(test_filter_empty_after_clear);
	run_test(test_double_clear_is_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
