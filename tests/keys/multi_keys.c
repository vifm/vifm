#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

extern int last;

TEST(multikeys_only)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"m"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"ma")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"mb")));
}

TEST(multikeys_and_full_command)
{
	last = 0;
	assert_int_equal(0, execute_keys(L"mbj"));
	assert_int_equal(2, last);
}

TEST(cancel_on_ctrl_c_and_escape)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"m"));
	assert_int_equal(0, execute_keys(L"m"L"\x03"));
	assert_int_equal(0, execute_keys(L"m"L"\x1b"));
}

TEST(rhs_multikeys_only)
{
	assert_int_equal(0, add_user_keys(L"a", L"mg", NORMAL_MODE, 1));
	assert_int_equal(0, execute_keys(L"a"));
}

TEST(rhs_multikeys_and_full_command)
{
	assert_int_equal(0, add_user_keys(L"b", L"mbj", NORMAL_MODE, 1));
	last = 0;
	assert_int_equal(0, execute_keys(L"b"));
	assert_int_equal(2, last);
}

TEST(rhs_multikeys_and_partial_command)
{
	assert_int_equal(0, add_user_keys(L"b", L"mbg", NORMAL_MODE, 1));
	last = 0;
	assert_true(IS_KEYS_RET_CODE(execute_keys(L"b")));
	assert_int_equal(0, last);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
