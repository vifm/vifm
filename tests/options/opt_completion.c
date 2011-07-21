#include "seatest.h"

#include "../../src/options.h"

static void
test_space_at_the_end(void)
{
	char *start;
	char *completed;

	completed = complete_options("fusehome=a\\ b\\ ", &start);
	assert_string_equal("fusehome=a\\ b\\ ", completed);
	free(completed);

	completed = complete_options("fusehome=a\\ b ", &start);
	assert_string_equal("fastrun", completed);
	free(completed);
}

static void
test_one_choice_opt(void)
{
	char *start;
	char *completed;

	completed = complete_options("fuse", &start);
	assert_string_equal("fusehome", completed);
	free(completed);

	completed = complete_options(NULL, &start);
	assert_string_equal("fusehome", completed);
	free(completed);

	completed = complete_options(NULL, &start);
	assert_string_equal("fusehome", completed);
	free(completed);
}

static void
test_one_choice_val(void)
{
	char *start;
	char *completed;

	completed = complete_options("sort=n", &start);
	assert_string_equal("sort=name", completed);
	free(completed);

	completed = complete_options(NULL, &start);
	assert_string_equal("sort=name", completed);
	free(completed);

	completed = complete_options(NULL, &start);
	assert_string_equal("sort=name", completed);
	free(completed);
}

static void
test_invalid_input(void)
{
	char *start;
	char *completed;

	completed = complete_options("fast ?f", &start);
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
