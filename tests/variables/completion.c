#include <stdlib.h>

#include "seatest.h"

#include "../../src/engine/completion.h"
#include "../../src/engine/variables.h"

static void
test_vars_empty_completion(void)
{
	char buf[] = "";
	const char *start;
	char *completed;

	complete_variables(buf, &start);
	assert_true(&buf[0] == start);
	completed = next_completion();
	assert_string_equal("", completed);
	free(completed);
}

static void
test_vars_completion(void)
{
	char buf[] = "abc";
	const char *start;
	char *completed;

	complete_variables(buf, &start);
	assert_true(&buf[0] == start);
	completed = next_completion();
	assert_string_equal("abc", completed);
	free(completed);
}

static void
test_envvars_completion(void)
{
	char buf[] = "$VAR_";
	const char *start;
	char *completed;

	complete_variables(buf, &start);
	assert_true(&buf[1] == start);

	completed = next_completion();
	assert_string_equal("VAR_A", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("VAR_B", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("VAR_C", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("VAR_", completed);
	free(completed);
}

static void
test_do_not_complete_removed_variables(void)
{
	char buf[] = "$VAR_";
	const char *start;
	char *completed;

	assert_int_equal(0, unlet_variables("$VAR_B"));

	complete_variables(buf, &start);
	assert_true(&buf[1] == start);

	completed = next_completion();
	assert_string_equal("VAR_A", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("VAR_C", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("VAR_", completed);
	free(completed);
}

void
completion_tests(void)
{
	test_fixture_start();

	run_test(test_vars_empty_completion);
	run_test(test_vars_completion);
	run_test(test_envvars_completion);
	run_test(test_do_not_complete_removed_variables);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
