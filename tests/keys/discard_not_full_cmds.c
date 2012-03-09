#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void
add_custom_keys(void)
{
	add_user_keys(L"ui", L"k", NORMAL_MODE, 0);
}

static void
wailt_full_command(void)
{
	assert_int_equal(KEYS_WAIT, execute_keys(L"u"));
	assert_int_equal(KEYS_UNKNOWN, execute_keys(L"ua"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"ui")));
}

void
discard_not_full_cmds(void)
{
	test_fixture_start();

	fixture_setup(add_custom_keys);

	run_test(wailt_full_command);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
