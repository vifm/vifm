#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void
dont_tread_all_cmds_as_motions(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"ZZ")));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"dZZ"));
}

static void
test_com_sel_com(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"gu"));
	assert_int_equal(0, execute_keys(L"guu"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"gug"));
	assert_int_equal(0, execute_keys(L"gugu"));
}

void
diff_motions(void)
{
	test_fixture_start();

	run_test(dont_tread_all_cmds_as_motions);
	run_test(test_com_sel_com);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
