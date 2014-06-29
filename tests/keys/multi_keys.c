#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

extern int last;

static void
test_multikeys_only(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"m"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"ma")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"mb")));
}

static void
test_multikeys_and_full_command(void)
{
	last = 0;
	assert_int_equal(0, execute_keys(L"mbj"));
	assert_int_equal(2, last);
}

static void
test_cancel_on_ctrl_c_and_escape(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"m"));
	assert_int_equal(0, execute_keys(L"m"L"\x03"));
	assert_int_equal(0, execute_keys(L"m"L"\x1b"));
}

static void
test_rhs_multikeys_only(void)
{
	assert_int_equal(0, add_user_keys(L"a", L"mg", NORMAL_MODE, 1));
	assert_int_equal(0, execute_keys(L"a"));
}

static void
test_rhs_multikeys_and_full_command(void)
{
	assert_int_equal(0, add_user_keys(L"b", L"mbj", NORMAL_MODE, 1));
	last = 0;
	assert_int_equal(0, execute_keys(L"b"));
	assert_int_equal(2, last);
}

static void
test_rhs_multikeys_and_partial_command(void)
{
	assert_int_equal(0, add_user_keys(L"b", L"mbg", NORMAL_MODE, 1));
	last = 0;
	assert_true(IS_KEYS_RET_CODE(execute_keys(L"b")));
	assert_int_equal(0, last);
}

void
multi_keys(void)
{
	test_fixture_start();

	run_test(test_multikeys_only);
	run_test(test_multikeys_and_full_command);
	run_test(test_cancel_on_ctrl_c_and_escape);
	run_test(test_rhs_multikeys_only);
	run_test(test_rhs_multikeys_and_full_command);
	run_test(test_rhs_multikeys_and_partial_command);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
