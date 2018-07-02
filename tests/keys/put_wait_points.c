#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

SETUP()
{
	vle_keys_user_add(L"abcdef", L"k", NORMAL_MODE, KEYS_FLAG_NONE);
}

TEST(abcdef)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"a"));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"ab"));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"abc"));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"abcd"));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"abcde"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"abcdef")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
