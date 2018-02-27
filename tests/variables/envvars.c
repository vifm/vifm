#include <stic.h>

#include <stdlib.h>

#include "../../src/engine/functions.h"
#include "../../src/engine/variables.h"
#include "../../src/utils/env.h"
#include "../../src/utils/utils.h"

#define VAR_NAME "VAR"

static var_t dummy(const call_info_t *call_info);

SETUP_ONCE()
{
	static const function_t function_a = { "a", "descr", {1,1}, &dummy };
	assert_success(function_register(&function_a));
}

SETUP()
{
	env_remove(VAR_NAME);
}

static var_t
dummy(const call_info_t *call_info)
{
	return var_from_str("");
}

TEST(env_variable_creation_success)
{
	assert_true(getenv(VAR_NAME) == NULL);
	assert_int_equal(0, let_variables("$" VAR_NAME "='VAL'"));
	assert_true(getenv(VAR_NAME) != NULL);
	if(getenv(VAR_NAME) != NULL)
	{
		assert_string_equal("VAL", getenv(VAR_NAME));
	}
}

TEST(env_variable_changing)
{
	assert_true(getenv(VAR_NAME) == NULL);
	assert_int_equal(0, let_variables("$" VAR_NAME "='VAL'"));
	assert_true(getenv(VAR_NAME) != NULL);
	if(getenv(VAR_NAME) != NULL)
	{
		assert_string_equal("VAL", getenv(VAR_NAME));
	}

	assert_int_equal(0, let_variables("$" VAR_NAME "='VAL2'"));
	assert_true(getenv(VAR_NAME) != NULL);
	if(getenv(VAR_NAME) != NULL)
	{
		assert_string_equal("VAL2", getenv(VAR_NAME));
	}
}

TEST(env_variable_addition_to_empty)
{
	assert_true(getenv(VAR_NAME) == NULL);
	assert_int_equal(0, let_variables("$" VAR_NAME ".='VAL2'"));
	assert_true(getenv(VAR_NAME) != NULL);
	if(getenv(VAR_NAME) != NULL)
	{
		assert_string_equal("VAL2", getenv(VAR_NAME));
	}
}

TEST(env_variable_addition)
{
	assert_true(getenv(VAR_NAME) == NULL);
	assert_int_equal(0, let_variables("$" VAR_NAME ".='VAL2'"));
	assert_true(getenv(VAR_NAME) != NULL);
	if(getenv(VAR_NAME) != NULL)
	{
		assert_string_equal("VAL2", getenv(VAR_NAME));
	}

	assert_int_equal(0, let_variables("$" VAR_NAME ".='VAL2'"));
	if(getenv(VAR_NAME) != NULL)
	{
		assert_string_equal("VAL2VAL2", getenv(VAR_NAME));
	}
}

TEST(env_variable_removal)
{
	assert_true(getenv("VAR_B") != NULL);
	assert_int_equal(0, unlet_variables("$VAR_B"));
	assert_true(getenv("VAR_B") == NULL);
}

TEST(env_variable_multiple_removal)
{
	assert_true(getenv("VAR_B") != NULL);
	assert_true(getenv("VAR_C") != NULL);
	assert_int_equal(0, unlet_variables("$VAR_B $VAR_C"));
	assert_true(getenv("VAR_B") == NULL);
	assert_true(getenv("VAR_C") == NULL);
}

TEST(unlet_with_equal_sign)
{
	assert_true(getenv("VAR_B") != NULL);
	assert_false(unlet_variables("$VAR_B=") == 0);
	assert_true(getenv("VAR_B") != NULL);
}

TEST(let_survives_wrong_argument_list_in_rhs)
{
	assert_failure(let_variables("$" VAR_NAME " = a(b)"));
}

TEST(variable_is_not_set_on_error)
{
	assert_null(getenv(VAR_NAME));
	assert_failure(let_variables("$" VAR_NAME " = a(b)"));
	assert_null(getenv(VAR_NAME));
}

TEST(increment_does_not_work_for_envvars)
{
	assert_failure(let_variables("$" VAR_NAME " += 3"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
