#include <stic.h>

#include <stdlib.h>

#include "../../src/engine/variables.h"
#include "../../src/utils/env.h"
#include "../../src/utils/utils.h"

#define VAR_NAME "VAR"

SETUP()
{
	env_remove(VAR_NAME);
}

TEST(envvar_remove_on_clear)
{
	assert_true(getenv(VAR_NAME) == NULL);
	assert_int_equal(0, let_variables("$" VAR_NAME "='VAL'"));
	assert_true(getenv(VAR_NAME) != NULL);

	clear_variables();

	assert_true(getenv(VAR_NAME) == NULL);
}

TEST(envvar_reset_on_clear)
{
	assert_true(getenv("VAR_A") != NULL);
	if(getenv("VAR_A") != NULL)
	{
		assert_string_equal("VAL_A", getenv("VAR_A"));
	}

	assert_int_equal(0, let_variables("$VAR_A='VAL_2'"));
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

TEST(builtinvvar_survive_clear_envvars)
{
	assert_success(setvar("v:test", var_from_bool(1)));
	clear_envvars();
	assert_true(getvar("v:test").type != VTYPE_ERROR);
}

TEST(builtinvvar_do_not_survive_clear_variables)
{
	assert_success(setvar("v:test", var_from_bool(1)));
	clear_variables();
	assert_true(getvar("v:test").type == VTYPE_ERROR);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
