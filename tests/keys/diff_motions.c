#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

TEST(dont_tread_all_cmds_as_motions)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"ZZ")));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"dZZ"));
}

TEST(com_sel_com)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"gu"));
	assert_int_equal(0, execute_keys(L"guu"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"gug"));
	assert_int_equal(0, execute_keys(L"gugu"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
