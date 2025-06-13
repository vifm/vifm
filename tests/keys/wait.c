#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

#include "suite.h"

TEST(wait_flag_turns_short_wait_into_wait)
{
	assert_success(set_user_key(L"vj", L"j", NORMAL_MODE));
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"v"));
	assert_success(vle_keys_user_add(L"vj", L"j", "descr", NORMAL_MODE,
				KEYS_FLAG_WAIT));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"v"));
	assert_success(set_user_key(L"vj", L"j", NORMAL_MODE));
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"v"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
