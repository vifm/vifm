#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */

#include "seatest.h"

#include "../../src/engine/completion.h"
#include "../../src/utils/str.h"

static char *
add_dquotes(const char match[])
{
	return format_str("\"%s\"", match);
}

static void
test_set_add_hook_sets_hook(void)
{
	char *match;

	compl_set_add_hook(&add_dquotes);

	assert_int_equal(0, vle_compl_add_match("a b c"));
	assert_int_equal(0, vle_compl_add_match("a b"));
	assert_int_equal(0, vle_compl_add_match("a"));
	completion_group_end();
	assert_int_equal(3, get_completion_count());

	match = next_completion();
	assert_string_equal("\"a b c\"", match);
	free(match);

	match = next_completion();
	assert_string_equal("\"a b\"", match);
	free(match);

	match = next_completion();
	assert_string_equal("\"a\"", match);
	free(match);

	compl_set_add_hook(NULL);
}

static void
test_set_add_hook_resets_hook(void)
{
	char *match;

	compl_set_add_hook(&add_dquotes);
	assert_int_equal(0, vle_compl_add_match("a b"));
	assert_int_equal(0, vle_compl_add_match("x y"));

	compl_set_add_hook(NULL);
	assert_int_equal(0, vle_compl_add_match("a b c"));

	completion_group_end();
	assert_int_equal(3, get_completion_count());

	match = next_completion();
	assert_string_equal("\"a b\"", match);
	free(match);

	match = next_completion();
	assert_string_equal("\"x y\"", match);
	free(match);

	match = next_completion();
	assert_string_equal("a b c", match);
	free(match);
}

static void
test_last_match_is_not_pre_processed(void)
{
	char *match;

	compl_set_add_hook(&add_dquotes);
	assert_int_equal(0, vle_compl_add_match("a b"));
	assert_int_equal(0, vle_compl_add_match("a b c"));
	completion_group_end();
	assert_int_equal(0, vle_compl_add_last_match("a"));

	assert_int_equal(3, get_completion_count());

	match = next_completion();
	assert_string_equal("\"a b c\"", match);
	free(match);

	match = next_completion();
	assert_string_equal("\"a b\"", match);
	free(match);

	match = next_completion();
	assert_string_equal("a", match);
	free(match);
}

void
add_hook_tests(void)
{
	test_fixture_start();

	run_test(test_set_add_hook_sets_hook);
	run_test(test_set_add_hook_resets_hook);
	run_test(test_last_match_is_not_pre_processed);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
