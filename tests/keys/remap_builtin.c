#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

TEST(remap_builtin_cmds)
{
	assert_success(vle_keys_user_add(L"ZZ", L":strange movement", NORMAL_MODE,
				KEYS_FLAG_NONE));
}

TEST(remap_builtin_waitpoint)
{
	assert_success(vle_keys_user_add(L"q", L"m", NORMAL_MODE, KEYS_FLAG_NONE));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"q"));
	assert_success(vle_keys_exec(L"qa"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
