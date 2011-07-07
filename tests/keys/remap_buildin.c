#include "seatest.h"

#include "../../src/keys.h"
#include "../../src/modes.h"

static void
remap_builtin_cmds(void)
{
	assert_int_equal(0, add_user_keys(L"ZZ", L":strange movement", NORMAL_MODE));
}

void
remap_buildin(void)
{
	test_fixture_start();

	run_test(remap_builtin_cmds);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
