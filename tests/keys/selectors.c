#include "seatest.h"

#include "../../src/engine/keys.h"

extern int last_command_count;
extern int last_selector_count;

/* This should be a macro to see what test have failed. */
#define check(wait, full, sel_count, cmd_count) \
	{ \
		assert_int_equal(KEYS_WAIT, execute_keys(wait)); \
		assert_int_equal(0, execute_keys(full)); \
		assert_int_equal((sel_count), last_selector_count); \
		assert_int_equal((cmd_count), last_command_count); \
	}

static void
test_no_number_ok(void)
{
	check(L"d", L"dk", NO_COUNT_GIVEN, NO_COUNT_GIVEN);
}

static void
test_with_number_ok(void)
{
	check(L"d1",   L"d1k",   1*1,   NO_COUNT_GIVEN);
	check(L"d12",  L"d12k",  1*12,  NO_COUNT_GIVEN);
	check(L"d123", L"d123k", 1*123, NO_COUNT_GIVEN);
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
	check(L"2d1",   L"2d1k",   2*1,   NO_COUNT_GIVEN);
	check(L"3d12",  L"3d12k",  3*12,  NO_COUNT_GIVEN);
	check(L"2d123", L"2d123k", 2*123, NO_COUNT_GIVEN);
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
