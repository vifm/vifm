#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void
add_custom_keys(void)
{
	add_user_keys(L"hi", L"j", NORMAL_MODE, 0);
	add_user_keys(L"hi2", L"hi", NORMAL_MODE, 0);

	add_user_keys(L"ho", L"j", NORMAL_MODE, 0);
	add_user_keys(L"ha2", L"ho", NORMAL_MODE, 0);
}

static void
user_key_chain(void)
{
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"ha2")));
}

static void
user_key_chain_wait(void)
{
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"hi2"));
}

void
users_key_to_key(void)
{
	test_fixture_start();

	fixture_setup(add_custom_keys);

	run_test(user_key_chain);
	run_test(user_key_chain_wait);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
