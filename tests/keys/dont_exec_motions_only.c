#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void
dont_exec_motions(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"ds")));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"s"));
}

void
dont_exec_motions_only(void)
{
	test_fixture_start();

	run_test(dont_exec_motions);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
