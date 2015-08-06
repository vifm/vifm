#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

SETUP()
{
	add_user_keys(L"ui", L"k", NORMAL_MODE, 0);
}

TEST(wait_full_command)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"u"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"ua"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"ui")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
