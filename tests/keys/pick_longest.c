#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

SETUP()
{
	vle_keys_user_add(L"a", L"z", NORMAL_MODE, KEYS_FLAG_NONE);
	vle_keys_user_add(L"ab", L"x", NORMAL_MODE, KEYS_FLAG_NONE);
	vle_keys_user_add(L"abc", L"k", NORMAL_MODE, KEYS_FLAG_NONE);

	vle_keys_user_add(L"q", L"k", NORMAL_MODE, KEYS_FLAG_NONE);
	vle_keys_user_add(L"qb", L"k", NORMAL_MODE, KEYS_FLAG_NONE);
	vle_keys_user_add(L"qbc", L"k", NORMAL_MODE, KEYS_FLAG_NONE);
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
