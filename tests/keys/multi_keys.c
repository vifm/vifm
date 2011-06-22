#include "seatest.h"

#include "../../src/keys.h"
#include "../../src/modes.h"

static void
multikeys(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"m"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"ma")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"mb")));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"mba"));
}

void
multi_keys(void)
{
	test_fixture_start();

	run_test(multikeys);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
