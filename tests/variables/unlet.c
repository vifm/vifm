#include "seatest.h"

#include "../../src/engine/variables.h"
#include "../../src/utils/env.h"

#define VAR_NAME_BASE "VAR"

static void
setup(void)
{
	env_remove(VAR_NAME_BASE "1");
	env_remove(VAR_NAME_BASE "2");
	env_remove(VAR_NAME_BASE "3");
}

static void
test_envvar_table_updates_do_not_crash(void)
{
	assert_int_equal(0, let_variables("$" VAR_NAME_BASE "1='VAL'"));
	assert_int_equal(0, unlet_variables("$" VAR_NAME_BASE "1"));
	assert_int_equal(0, let_variables("$" VAR_NAME_BASE "2='VAL'"));
	assert_int_equal(0, let_variables("$" VAR_NAME_BASE "3='VAL'"));
}

void
unlet_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_envvar_table_updates_do_not_crash);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
