#include <stic.h>

#include "../../src/engine/keys.h"

TEST(multi_as_a_motion)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"'"));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"d"));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"d'"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"d'm")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
