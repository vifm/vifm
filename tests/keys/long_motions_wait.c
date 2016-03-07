#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

TEST(multichar_selector)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"d"));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"di"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"dif")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
