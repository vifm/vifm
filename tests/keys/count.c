#include "seatest.h"

#include <limits.h> /* INT_MAX */
#include <stdlib.h> /* free() */
#include <wchar.h> /* wchar_t */

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/utils/str.h"

extern int last_command_count;

static void
add_custom_keys(void)
{
	add_user_keys(L"abc", L"", NORMAL_MODE, 0);
}

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

static void
test_max_count_is_handled_correctly(void)
{
	char *const keys = format_str("%udd", INT_MAX);
	wchar_t *const keysw = to_wide(keys);

	assert_false(IS_KEYS_RET_CODE(execute_keys(keysw)));
	assert_int_equal(INT_MAX, last_command_count);

	free(keysw);
	free(keys);
}

static void
test_nops_count_not_passed(void)
{
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"10abcdd")));
	assert_int_equal(NO_COUNT_GIVEN, last_command_count);
}

void
count_tests(void)
{
	test_fixture_start();

	fixture_setup(&add_custom_keys);

	run_test(test_no_number_get_not_def);
	run_test(test_normal_number_get_it);
	run_test(test_huge_number_get_intmax);
	run_test(test_max_count_is_handled_correctly);
	run_test(test_nops_count_not_passed);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
