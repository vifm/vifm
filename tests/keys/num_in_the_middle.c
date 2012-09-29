#include "seatest.h"

#include "../../src/engine/keys.h"

extern int last_command_count;

static void
test_no_number_ok(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L""));
	assert_int_equal(0, execute_keys(L"<"));
	assert_int_equal(NO_COUNT_GIVEN, last_command_count);
}

static void
test_with_number_ok(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"1"));
	assert_int_equal(0, execute_keys(L"1<"));
	assert_int_equal(1*1, last_command_count);

	assert_int_equal(KEYS_WAIT, execute_keys(L"12"));
	assert_int_equal(0, execute_keys(L"12<"));
	assert_int_equal(1*12, last_command_count);

	assert_int_equal(KEYS_WAIT, execute_keys(L"123"));
	assert_int_equal(0, execute_keys(L"123<"));
	assert_int_equal(1*123, last_command_count);
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

static void
test_with_number_before_and_in_the_middle_ok(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"21"));
	assert_int_equal(0, execute_keys(L"21<"));
	assert_int_equal(2*1, last_command_count);

	assert_int_equal(KEYS_WAIT, execute_keys(L"112"));
	assert_int_equal(0, execute_keys(L"312<"));
	assert_int_equal(3*12, last_command_count);

	assert_int_equal(KEYS_WAIT, execute_keys(L"2123"));
	assert_int_equal(0, execute_keys(L"2123<"));
	assert_int_equal(2*123, last_command_count);
}

void
num_in_the_middle_tests(void)
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
