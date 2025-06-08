#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

#include "suite.h"

SETUP()
{
	assert_success(set_user_key(L"a", L"z", NORMAL_MODE));
	assert_success(set_user_key(L"ab", L"x", NORMAL_MODE));
	assert_success(set_user_key(L"abc", L"k", NORMAL_MODE));

	assert_success(set_user_key(L"q", L"k", NORMAL_MODE));
	assert_success(set_user_key(L"qb", L"k", NORMAL_MODE));
	assert_success(set_user_key(L"qbc", L"k", NORMAL_MODE));
}

TEST(when_previous_unknown)
{
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"a"));
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"ab"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"abc")));
}

TEST(when_previous_known)
{
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"q"));
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"qb"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"qbc")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
