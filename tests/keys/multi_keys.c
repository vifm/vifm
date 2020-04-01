#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

#include "builtin_keys.h"

TEST(multikeys_only)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"m"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"ma")));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"mb")));
}

TEST(multikeys_and_full_command)
{
	last = 0;
	assert_success(vle_keys_exec(L"mbj"));
	assert_int_equal(2, last);
}

TEST(cancel_on_ctrl_c_and_escape)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"m"));
	assert_success(vle_keys_exec(L"m"L"\x03"));
	assert_success(vle_keys_exec(L"m"L"\x1b"));
}

TEST(rhs_multikeys_only)
{
	assert_success(vle_keys_user_add(L"a", L"mg", NORMAL_MODE,
				KEYS_FLAG_NOREMAP));
	assert_success(vle_keys_exec(L"a"));
}

TEST(rhs_multikeys_and_full_command)
{
	assert_success(vle_keys_user_add(L"b", L"mbj", NORMAL_MODE,
				KEYS_FLAG_NOREMAP));
	last = 0;
	assert_success(vle_keys_exec(L"b"));
	assert_int_equal(2, last);
}

TEST(rhs_multikeys_and_partial_command)
{
	assert_success(vle_keys_user_add(L"b", L"mbg", NORMAL_MODE,
				KEYS_FLAG_NOREMAP));
	last = 0;
	assert_true(IS_KEYS_RET_CODE(vle_keys_exec(L"b")));
	assert_int_equal(0, last);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
