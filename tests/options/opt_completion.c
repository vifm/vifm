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
	assert_string_equal("a\\ b\\ ", completed);
	free(completed);

	reset_completion();
	complete_options("fusehome=a\\ b ", &start);

	completed = next_completion();
	assert_string_equal("all", completed);
	free(completed);

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

static void
test_skip_abbreviations(void)
{
	const char *start;
	char *completed;

	reset_completion();
	complete_options("", &start);

	completed = next_completion();
	assert_string_equal("all", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("fastrun", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("fusehome", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("sort", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("sortorder", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("vifminfo", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("", completed);
	free(completed);
}

static void
test_expand_abbreviations(void)
{
	const char *start;
	char *completed;

	reset_completion();
	complete_options("fr", &start);

	completed = next_completion();
	assert_string_equal("fastrun", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("fastrun", completed);
	free(completed);
}

static void
test_after_eq_value_completion(void)
{
	char buf[] = "vifminfo=op";
	const char *start;
	char *completed;

	reset_completion();
	complete_options(buf, &start);
	assert_true(start == buf + 9);

	completed = next_completion();
	assert_string_equal("options", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("options", completed);
	free(completed);
}

static void
test_after_meq_value_completion(void)
{
	char buf[] = "vifminfo-=op";
	const char *start;
	char *completed;

	reset_completion();
	complete_options(buf, &start);
	assert_true(start == buf + 10);

	completed = next_completion();
	assert_string_equal("options", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("options", completed);
	free(completed);
}

static void
test_after_peq_value_completion(void)
{
	char buf[] = "vifminfo+=op";
	const char *start;
	char *completed;

	reset_completion();
	complete_options(buf, &start);
	assert_true(start == buf + 10);

	completed = next_completion();
	assert_string_equal("options", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("options", completed);
	free(completed);
}

static void
test_set_values_completion(void)
{
	char buf[] = "vifminfo=tui,c";
	const char *start;
	char *completed;

	reset_completion();
	complete_options(buf, &start);
	assert_true(start == buf + 13);

	completed = next_completion();
	assert_string_equal("commands", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("cs", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("c", completed);
	free(completed);
}

static void
test_colon(void)
{
	const char *start;
	char *completed;

	reset_completion();
	complete_options("fusehome:a\\ b\\ ", &start);
	completed = next_completion();
	assert_string_equal("a\\ b\\ ", completed);
	free(completed);

	reset_completion();
	complete_options("fusehome:a\\ b ", &start);

	completed = next_completion();
	assert_string_equal("all", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("fastrun", completed);
	free(completed);
}

static void
test_umbiguous_beginning(void)
{
	const char *start;
	char *completed;

	reset_completion();
	complete_options("sort", &start);

	completed = next_completion();
	assert_string_equal("sort", completed);
	free(completed);

	completed = next_completion();
	assert_string_equal("sortorder", completed);
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
	run_test(test_skip_abbreviations);
	run_test(test_expand_abbreviations);
	run_test(test_after_eq_value_completion);
	run_test(test_after_meq_value_completion);
	run_test(test_after_peq_value_completion);
	run_test(test_set_values_completion);
	run_test(test_colon);
	run_test(test_umbiguous_beginning);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
