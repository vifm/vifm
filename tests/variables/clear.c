#include <stdlib.h>

#include "seatest.h"

#include "../../src/engine/variables.h"
#include "../../src/utils/utils.h"

#define VAR_NAME "VAR"

static void
setup(void)
{
	env_remove(VAR_NAME);
}

static void
test_envvar_remove_on_clear(void)
{
	assert_true(getenv(VAR_NAME) == NULL);
	assert_int_equal(0, let_variable("$" VAR_NAME "='VAL'"));
	assert_true(getenv(VAR_NAME) != NULL);

	clear_variables();

	assert_true(getenv(VAR_NAME) == NULL);
}

static void
test_envvar_reset_on_clear(void)
{
	assert_true(getenv("VAR_A") != NULL);
	if(getenv("VAR_A") != NULL)
	{
		assert_string_equal("VAL_A", getenv("VAR_A"));
	}

	assert_int_equal(0, let_variable("$VAR_A='VAL_2'"));
	assert_true(getenv("VAR_A") != NULL);
	if(getenv("VAR_A") != NULL)
	{
		assert_string_equal("VAL_2", getenv("VAR_A"));
	}

	clear_variables();

	assert_true(getenv("VAR_A") != NULL);
	if(getenv("VAR_A") != NULL)
	{
		assert_string_equal("VAL_A", getenv("VAR_A"));
	}
}

void
clear_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_envvar_remove_on_clear);
	run_test(test_envvar_reset_on_clear);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
