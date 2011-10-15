#include "seatest.h"

#include "../../src/keys.h"
#include "../../src/modes.h"

static void
test_no_number(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L""));
	assert_int_equal(0, execute_keys(L"<"));
}

static void
test_with_number(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"0"));
	assert_int_equal(0, execute_keys(L"0<"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"01"));
	assert_int_equal(0, execute_keys(L"01<"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"012"));
	assert_int_equal(0, execute_keys(L"012<"));
}

void
num_in_the_middle_tests(void)
{
	test_fixture_start();

	run_test(test_no_number);
	run_test(test_with_number);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
