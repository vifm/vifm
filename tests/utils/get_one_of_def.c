#include <stic.h>

#include <stdlib.h>
#include <string.h>

#include "../../src/utils/env.h"

#define DEFAULT_VAL "default"
#define VAR_A       "VAR_A"
#define VAR_A_VAL   "VAR_A_VAL"
#define VAR_B       "VAR_B"
#define VAR_B_VAL   "VAR_B_VAL"

SETUP()
{
	env_remove(VAR_A);
	env_remove(VAR_B);
}

TEST(empty_list_returns_def)
{
	const char *result;
	result = env_get_one_of_def(DEFAULT_VAL, (const char *)NULL);
	assert_string_equal(DEFAULT_VAL, result);
}

TEST(none_exist_returns_def)
{
	const char *result;
	result = env_get_one_of_def(DEFAULT_VAL, VAR_A, VAR_B, (const char *)NULL);
	assert_string_equal(DEFAULT_VAL, result);
}

TEST(last_exists_returns_it)
{
	const char *result;
	env_set(VAR_B, VAR_B_VAL);
	result = env_get_one_of_def(DEFAULT_VAL, VAR_A, VAR_B, (const char *)NULL);
	assert_string_equal(VAR_B_VAL, result);
}

TEST(all_exist_returns_first)
{
	const char *result;
	env_set(VAR_A, VAR_A_VAL);
	env_set(VAR_B, VAR_B_VAL);
	result = env_get_one_of_def(DEFAULT_VAL, VAR_A, VAR_B, (const char *)NULL);
	assert_string_equal(VAR_A_VAL, result);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
