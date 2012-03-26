#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void
multichar_motions(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"di"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"dif")));
}

void
long_motions_wait(void)
{
	test_fixture_start();

	run_test(multichar_motions);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
