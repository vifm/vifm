#include "seatest.h"

#include "../../src/engine/keys.h"

static void
multi_as_a_motion(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"'"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"d"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"d'"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"d'm")));
}

void
same_multi_and_motion(void)
{
	test_fixture_start();

	run_test(multi_as_a_motion);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
