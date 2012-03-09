#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static void
remap_builtin_cmds(void)
{
	assert_int_equal(0, add_user_keys(L"ZZ", L":strange movement", NORMAL_MODE,
			0));
}

static void
remap_builtin_waitpoint(void)
{
	assert_int_equal(0, add_user_keys(L"q", L"m", NORMAL_MODE, 0));
	assert_int_equal(KEYS_WAIT, execute_keys(L"q"));
	assert_int_equal(0, execute_keys(L"qa"));
}

void
remap_builtin(void)
{
	test_fixture_start();

	run_test(remap_builtin_cmds);
	run_test(remap_builtin_waitpoint);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
