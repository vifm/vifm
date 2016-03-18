#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

SETUP()
{
	vle_keys_user_add(L"hi", L"j", NORMAL_MODE, 0);
	vle_keys_user_add(L"hi2", L"hi", NORMAL_MODE, 0);

	vle_keys_user_add(L"ho", L"j", NORMAL_MODE, 0);
	vle_keys_user_add(L"ha2", L"ho", NORMAL_MODE, 0);
}

TEST(user_key_chain)
{
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"ha2")));
}

TEST(user_key_chain_wait)
{
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"hi2"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
