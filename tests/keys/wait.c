#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

TEST(wait_flag_turns_short_wait_into_wait)
{
	vle_keys_user_add(L"vj", L"j", NORMAL_MODE, KEYS_FLAG_NONE);
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"v"));
	vle_keys_user_add(L"vj", L"j", NORMAL_MODE, KEYS_FLAG_WAIT);
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"v"));
	vle_keys_user_add(L"vj", L"j", NORMAL_MODE, KEYS_FLAG_NONE);
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"v"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
