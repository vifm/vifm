#include <stdlib.h>

#include "seatest.h"

#include "../../src/completion.h"
#include "../../src/options.h"

static void
test_space_at_the_end(void)
{
	const char *start;
	char *completed;

	reset_completion();
	complete_options("fusehome=a\\ b\\ ", &start);
	completed = next_completion();
	assert_string_equal("fusehome=a\\ b\\ ", completed);
	free(completed);

	reset_completion();
	complete_options("fusehome=a\\ b ", &start);
	completed = next_completion();
	assert_string_equal("fastrun", completed);
	free(completed);
}

static void
test_one_choice_opt(void)
{
	const char *start;
	char *completed;

	reset_completion();
	complete_options("fuse", &start);

	completed = next_completion();
	assert_string_equal("fusehome", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("fusehome", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("fusehome", completed);
	free(completed);
}

static void
test_one_choice_val(void)
{
	char buf[] = "sort=n";
	const char *start;
	char *completed;

	reset_completion();
	complete_options(buf, &start);
	assert_true(start == buf + 5);

	completed = next_completion();
	assert_string_equal("name", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("name", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("name", completed);
	free(completed);
}

static void
test_invalid_input(void)
{
	const char *start;
	char *completed;

	reset_completion();
	complete_options("fast ?f", &start);
	completed = next_completion();
	assert_string_equal("fast ?f", completed);
	free(completed);
}

void
opt_completion(void)
{
	test_fixture_start();

	run_test(test_space_at_the_end);
	run_test(test_one_choice_opt);
	run_test(test_one_choice_val);
	run_test(test_invalid_input);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
