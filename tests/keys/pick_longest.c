#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void
setup(void)
{
	add_user_keys(L"a", L"z", NORMAL_MODE, 0);
	add_user_keys(L"ab", L"x", NORMAL_MODE, 0);
	add_user_keys(L"abc", L"k", NORMAL_MODE, 0);

	add_user_keys(L"q", L"k", NORMAL_MODE, 0);
	add_user_keys(L"qb", L"k", NORMAL_MODE, 0);
	add_user_keys(L"qbc", L"k", NORMAL_MODE, 0);
}

static void
test_when_previous_unknown(void)
{
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"a"));
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"ab"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"abc")));
}

static void
test_when_previous_known(void)
{
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"q"));
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"qb"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"qbc")));
}

void
pick_longest(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_when_previous_unknown);
	run_test(test_when_previous_known);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
