#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

TEST(remap_builtin_cmds)
{
	assert_int_equal(0, add_user_keys(L"ZZ", L":strange movement", NORMAL_MODE,
			0));
}

TEST(remap_builtin_waitpoint)
{
	assert_int_equal(0, add_user_keys(L"q", L"m", NORMAL_MODE, 0));
	assert_int_equal(KEYS_WAIT, execute_keys(L"q"));
	assert_int_equal(0, execute_keys(L"qa"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
