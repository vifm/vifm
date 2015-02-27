#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

SETUP()
{
	add_user_keys(L"hi", L"j", NORMAL_MODE, 0);
	add_user_keys(L"hi2", L"hi", NORMAL_MODE, 0);

	add_user_keys(L"ho", L"j", NORMAL_MODE, 0);
	add_user_keys(L"ha2", L"ho", NORMAL_MODE, 0);
}

TEST(user_key_chain)
{
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"ha2")));
}

TEST(user_key_chain_wait)
{
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"hi2"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
