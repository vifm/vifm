#include <string.h>

#include "seatest.h"

#include "../../src/column_view.h"
#include "../../src/viewcolumns_parser.h"
#include "test.h"

static void
test_auto_ok(void)
{
	int result = do_parse("{name}");
	assert_true(result == 0);
	assert_true(info.sizing == ST_AUTO);
}

static void
test_absolute_without_limitation_ok(void)
{
	int result = do_parse("10{name}");
	assert_true(result == 0);
	assert_int_equal(10, info.full_width);
	assert_int_equal(10, info.text_width);
	assert_true(info.sizing == ST_ABSOLUTE);
}

static void
test_absolute_with_limitation_ok(void)
{
	int result = do_parse("10.8{name}");
	assert_true(result == 0);
	assert_int_equal(10, info.full_width);
	assert_int_equal(8, info.text_width);
	assert_true(info.sizing == ST_ABSOLUTE);
}

static void
test_absolute_with_limitation_overflow_fail(void)
{
	int result = do_parse("10.11{name}");
	assert_false(result == 0);
}

static void
test_percent_ok(void)
{
	int result = do_parse("50%{name}");
	assert_true(result == 0);
	assert_int_equal(50, info.full_width);
	assert_int_equal(50, info.text_width);
	assert_true(info.sizing == ST_PERCENT);
}

static void
test_percent_zero_fail(void)
{
	int result = do_parse("0%{name}");
	assert_false(result == 0);
}

static void
test_percent_greater_than_hundred_fail(void)
{
	int result = do_parse("110%{name}");
	assert_false(result == 0);
}

static void
test_percent_summ_greater_than_hundred_fail(void)
{
	int result = do_parse("50%{name},50%{name},50%{name}");
	assert_false(result == 0);
}

void
sizes_tests(void)
{
	test_fixture_start();

	run_test(test_auto_ok);
	run_test(test_absolute_without_limitation_ok);
	run_test(test_absolute_with_limitation_ok);
	run_test(test_absolute_with_limitation_overflow_fail);
	run_test(test_percent_ok);
	run_test(test_percent_zero_fail);
	run_test(test_percent_greater_than_hundred_fail);
	run_test(test_percent_summ_greater_than_hundred_fail);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
