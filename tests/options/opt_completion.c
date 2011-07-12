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
	assert_string_equal("fusehome", completed);
	free(completed);
}

void
opt_completion(void)
{
	test_fixture_start();

	run_test(test_space_at_the_end);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
