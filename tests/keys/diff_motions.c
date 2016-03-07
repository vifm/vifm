#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

TEST(dont_tread_all_cmds_as_motions)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"d"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"ZZ")));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"dZZ"));
}

TEST(com_sel_com)
{
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"gu"));
	assert_int_equal(0, vle_keys_exec(L"guu"));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"gug"));
	assert_int_equal(0, vle_keys_exec(L"gugu"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
