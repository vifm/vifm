#include "seatest.h"

#include "../../src/keys.h"
#include "../../src/modes.h"

static void
dont_remap_builtin_cmds(void)
{
	assert_int_equal(-1, add_user_keys(L"ZZ", L":strange movement", NORMAL_MODE));
}

void
dont_remap_buildin(void)
{
	test_fixture_start();

	run_test(dont_remap_builtin_cmds);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
