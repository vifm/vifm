#include "seatest.h"

#include <limits.h> /* INT_MAX */

#include "../../src/engine/keys.h"

extern int last_command_count;

static void
test_no_number_get_not_def(void)
{
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"dd")));
	assert_int_equal(NO_COUNT_GIVEN, last_command_count);
}

static void
test_normal_number_get_it(void)
{
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"123dd")));
	assert_int_equal(123, last_command_count);
}

static void
test_huge_number_get_intmax(void)
{
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"999999999999dd")));
	assert_int_equal(INT_MAX, last_command_count);
}

void
count_tests(void)
{
	test_fixture_start();

	run_test(test_no_number_get_not_def);
	run_test(test_normal_number_get_it);
	run_test(test_huge_number_get_intmax);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
