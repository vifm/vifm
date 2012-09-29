#include "seatest.h"

#include "../../src/engine/keys.h"

extern int last_selector_count;

static void
test_no_number_ok(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_int_equal(0, execute_keys(L"dk"));
	assert_int_equal(NO_COUNT_GIVEN, last_selector_count);
}

static void
test_with_number_ok(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d1"));
	assert_int_equal(0, execute_keys(L"d1k"));
	assert_int_equal(1*1, last_selector_count);

	assert_int_equal(KEYS_WAIT, execute_keys(L"d12"));
	assert_int_equal(0, execute_keys(L"d12k"));
	assert_int_equal(1*12, last_selector_count);

	assert_int_equal(KEYS_WAIT, execute_keys(L"d123"));
	assert_int_equal(0, execute_keys(L"d123k"));
	assert_int_equal(1*123, last_selector_count);
}

static void
test_with_zero_number_fail(void)
{
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"d0"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"d0k"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"d01"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"d01k"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"d012"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"d012k"));
}

static void
test_with_number_before_and_in_the_middle_ok(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"2d1"));
	assert_int_equal(0, execute_keys(L"2d1k"));
	assert_int_equal(2*1, last_selector_count);

	assert_int_equal(KEYS_WAIT, execute_keys(L"1d12"));
	assert_int_equal(0, execute_keys(L"3d12k"));
	assert_int_equal(3*12, last_selector_count);

	assert_int_equal(KEYS_WAIT, execute_keys(L"2d123"));
	assert_int_equal(0, execute_keys(L"2d123k"));
	assert_int_equal(2*123, last_selector_count);
}

void
selectors_tests(void)
{
	test_fixture_start();

	run_test(test_no_number_ok);
	run_test(test_with_number_ok);
	run_test(test_with_zero_number_fail);
	run_test(test_with_number_before_and_in_the_middle_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
