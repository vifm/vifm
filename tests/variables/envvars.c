#include <stdlib.h>

#include "seatest.h"

#include "../../src/utils.h"
#include "../../src/variables.h"

#define VAR_NAME "VAR"

static void
setup(void)
{
	env_remove(VAR_NAME);
}

static void
test_env_variable_creation_success(void)
{
	assert_true(getenv(VAR_NAME) == NULL);
	assert_int_equal(0, let_variable("$" VAR_NAME "='VAL'"));
	assert_true(getenv(VAR_NAME) != NULL);
	if(getenv(VAR_NAME) != NULL)
	{
		assert_string_equal("VAL", getenv(VAR_NAME));
	}
}

static void
test_env_variable_changing(void)
{
	test_env_variable_creation_success();

	assert_int_equal(0, let_variable("$" VAR_NAME "='VAL2'"));
	assert_true(getenv(VAR_NAME) != NULL);
	if(getenv(VAR_NAME) != NULL)
	{
		assert_string_equal("VAL2", getenv(VAR_NAME));
	}
}

static void
test_env_variable_addition_to_empty(void)
{
	assert_true(getenv(VAR_NAME) == NULL);
	assert_int_equal(0, let_variable("$" VAR_NAME ".='VAL2'"));
	assert_true(getenv(VAR_NAME) != NULL);
	if(getenv(VAR_NAME) != NULL)
	{
		assert_string_equal("VAL2", getenv(VAR_NAME));
	}
}

static void
test_env_variable_addition(void)
{
	test_env_variable_addition_to_empty();

	assert_int_equal(0, let_variable("$" VAR_NAME ".='VAL2'"));
	if(getenv(VAR_NAME) != NULL)
	{
		assert_string_equal("VAL2VAL2", getenv(VAR_NAME));
	}
}

static void
test_env_variable_removal(void)
{
	assert_true(getenv("VAR_B") != NULL);
	assert_int_equal(0, unlet_variables("$VAR_B"));
	assert_true(getenv("VAR_B") == NULL);
}

static void
test_env_variable_multiple_removal(void)
{
	assert_true(getenv("VAR_B") != NULL);
	assert_true(getenv("VAR_C") != NULL);
	assert_int_equal(0, unlet_variables("$VAR_B $VAR_C"));
	assert_true(getenv("VAR_B") == NULL);
	assert_true(getenv("VAR_C") == NULL);
}

void
envvars_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_env_variable_creation_success);
	run_test(test_env_variable_changing);
	run_test(test_env_variable_addition_to_empty);
	run_test(test_env_variable_addition);
	run_test(test_env_variable_removal);
	run_test(test_env_variable_multiple_removal);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
