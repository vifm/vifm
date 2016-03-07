#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

TEST(dont_exec_motions)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"d"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"ds")));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"s"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
