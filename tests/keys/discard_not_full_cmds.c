#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

SETUP()
{
	vle_keys_user_add(L"ui", L"k", NORMAL_MODE, KEYS_FLAG_NONE);
}

TEST(wait_full_command)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"u"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"ua"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"ui")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
