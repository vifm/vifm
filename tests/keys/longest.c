#include "seatest.h"

#include "../../src/engine/keys.h"

static void
treat_full_commands_right(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"\""));
	assert_int_equal(KEYS_WAIT, execute_keys(L"\"a"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"\"a1"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"\"a1d"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"\"a1d\""));
	assert_int_equal(KEYS_WAIT, execute_keys(L"\"a1d\"r"));
	assert_int_equal(KEYS_WAIT, execute_keys(L"\"a1d\"r1"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"\"a1d\"r1k")));
}

void
longest(void)
{
	test_fixture_start();

	run_test(treat_full_commands_right);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
