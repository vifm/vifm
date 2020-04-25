#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"

#include "builtin_keys.h"

SETUP()
{
	vle_keys_user_add(L"d", L"vz", VISUAL_MODE, KEYS_FLAG_NONE);
}

TEST(do_not_rerun_chain_on_failure)
{
	last = 0;
	vle_mode_set(VISUAL_MODE, VMT_PRIMARY);

	assert_int_equal(0, vle_keys_exec(L"dd"));

	assert_int_equal(0, last);
	assert_true(vle_mode_is(NORMAL_MODE));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
