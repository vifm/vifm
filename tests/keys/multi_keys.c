#include "seatest.h"

#include "../../src/engine/keys.h"

static void
multikeys(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"m"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"ma")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"mb")));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"mba"));
}

static void
test_cancel_on_ctrl_c_and_escape(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"m"));
	assert_int_equal(0, execute_keys(L"m"L"\x03"));
	assert_int_equal(0, execute_keys(L"m"L"\x1b"));
}

void
multi_keys(void)
{
	test_fixture_start();

	run_test(multikeys);
	run_test(test_cancel_on_ctrl_c_and_escape);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
