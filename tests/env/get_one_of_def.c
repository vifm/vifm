#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/utils/env.h"

#define DEFAULT_VAL "default"
#define VAR_A       "VAR_A"
#define VAR_A_VAL   "VAR_A_VAL"
#define VAR_B       "VAR_B"
#define VAR_B_VAL   "VAR_B_VAL"

static void
setup(void)
{
	env_remove(VAR_A);
	env_remove(VAR_B);
}

static void
test_empty_list_returns_def(void)
{
	const char *result;
	result = env_get_one_of_def(DEFAULT_VAL, NULL);
	assert_string_equal(DEFAULT_VAL, result);
}

static void
test_none_exist_returns_def(void)
{
	const char *result;
	result = env_get_one_of_def(DEFAULT_VAL, VAR_A, VAR_B, NULL);
	assert_string_equal(DEFAULT_VAL, result);
}

static void
test_last_exists_returns_it(void)
{
	const char *result;
	env_set(VAR_B, VAR_B_VAL);
	result = env_get_one_of_def(DEFAULT_VAL, VAR_A, VAR_B, NULL);
	assert_string_equal(VAR_B_VAL, result);
}

static void
test_all_exist_returns_first(void)
{
	const char *result;
	env_set(VAR_A, VAR_A_VAL);
	env_set(VAR_B, VAR_B_VAL);
	result = env_get_one_of_def(DEFAULT_VAL, VAR_A, VAR_B, NULL);
	assert_string_equal(VAR_A_VAL, result);
}

void
get_one_of_def_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_empty_list_returns_def);
	run_test(test_none_exist_returns_def);
	run_test(test_last_exists_returns_it);
	run_test(test_all_exist_returns_first);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
