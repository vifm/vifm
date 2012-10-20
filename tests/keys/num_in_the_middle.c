#include "seatest.h"

#include "../../src/engine/keys.h"

extern int last_command_count;

#define check(wait, full, cmd_count) \
	{ \
		assert_int_equal(KEYS_WAIT, execute_keys(wait)); \
		assert_int_equal(0, execute_keys(full)); \
		assert_int_equal((cmd_count), last_command_count); \
	}

static void
test_no_number_ok(void)
{
	check(L"", L"<", NO_COUNT_GIVEN);
}

static void
test_with_number_ok(void)
{
	check(L"1",   L"1<",   1*1);
	check(L"12",  L"12<",  1*12);
	check(L"123", L"123<", 1*123);
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
	check(L"21",   L"21<",   2*1);
	check(L"312",  L"312<",  3*12);
	check(L"2123", L"2123<", 2*123);
}

static void
test_dd_nim_ok(void)
{
	check(L"2d1",   L"2d1d",   2*1);
	check(L"3d12",  L"3d12d",  3*12);
	check(L"2d123", L"2d123d", 2*123);
}

void
num_in_the_middle_tests(void)
{
	test_fixture_start();

	run_test(test_no_number_ok);
	run_test(test_with_number_ok);
	run_test(test_with_zero_number_fail);
	run_test(test_with_number_before_and_in_the_middle_ok);
	run_test(test_dd_nim_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
