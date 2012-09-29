#include "seatest.h"

#include "../../src/engine/keys.h"

static void
test_no_number_ok(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L""));
	assert_int_equal(0, execute_keys(L"<"));
}

static void
test_with_number_ok(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"1"));
	assert_int_equal(0, execute_keys(L"1<"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"12"));
	assert_int_equal(0, execute_keys(L"12<"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"123"));
	assert_int_equal(0, execute_keys(L"123<"));
}

static void
test_with_zero_number_fail(void)
{
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"0"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"0<"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"01"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"01<"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"012"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"012<"));
}

void
num_in_the_middle_tests(void)
{
	test_fixture_start();

	run_test(test_no_number_ok);
	run_test(test_with_number_ok);
	run_test(test_with_zero_number_fail);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
