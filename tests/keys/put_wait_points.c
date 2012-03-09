#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void
add_custom_keys(void)
{
	add_user_keys(L"abcdef", L"k", NORMAL_MODE, 0);
}

static void
test_abcdef(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"a"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"ab"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"abc"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"abcd"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"abcde"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"abcdef")));
}

void
put_wait_points(void)
{
	test_fixture_start();

	fixture_setup(add_custom_keys);

	run_test(test_abcdef);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
