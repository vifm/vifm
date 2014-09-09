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
test_set_add_path_hook_sets_hook(void)
{
	char *match;

	vle_compl_set_add_path_hook(&add_dquotes);

	assert_int_equal(0, vle_compl_add_path_match("a b c"));
	assert_int_equal(0, vle_compl_add_path_match("a b"));
	assert_int_equal(0, vle_compl_add_path_match("a"));
	vle_compl_finish_group();
	assert_int_equal(3, vle_compl_get_count());

	match = vle_compl_next();
	assert_string_equal("\"a b c\"", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("\"a b\"", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("\"a\"", match);
	free(match);

	vle_compl_set_add_path_hook(NULL);
}

static void
test_set_add_path_hook_resets_hook(void)
{
	char *match;

	vle_compl_set_add_path_hook(&add_dquotes);
	assert_int_equal(0, vle_compl_add_path_match("a b"));
	assert_int_equal(0, vle_compl_add_path_match("x y"));

	vle_compl_set_add_path_hook(NULL);
	assert_int_equal(0, vle_compl_add_path_match("a b c"));

	vle_compl_finish_group();
	assert_int_equal(3, vle_compl_get_count());

	match = vle_compl_next();
	assert_string_equal("\"a b\"", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("\"x y\"", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("a b c", match);
	free(match);
}

static void
test_last_match_is_pre_processed(void)
{
	char *match;

	vle_compl_set_add_path_hook(&add_dquotes);
	assert_int_equal(0, vle_compl_add_path_match("a b"));
	assert_int_equal(0, vle_compl_add_path_match("a b c"));
	vle_compl_finish_group();
	assert_int_equal(0, vle_compl_add_last_path_match("a"));

	assert_int_equal(3, vle_compl_get_count());

	match = vle_compl_next();
	assert_string_equal("\"a b c\"", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("\"a b\"", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("\"a\"", match);
	free(match);
}

static void
test_set_add_path_hook_does_not_affect_non_path_functions(void)
{
	char *match;

	vle_compl_set_add_path_hook(&add_dquotes);
	assert_int_equal(0, vle_compl_add_match("a b"));
	assert_int_equal(0, vle_compl_add_match("a b c"));
	vle_compl_finish_group();
	assert_int_equal(0, vle_compl_add_last_match("a"));

	assert_int_equal(3, vle_compl_get_count());

	match = vle_compl_next();
	assert_string_equal("a b", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("a b c", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("a", match);
	free(match);
}

void
add_hook_tests(void)
{
	test_fixture_start();

	run_test(test_set_add_path_hook_sets_hook);
	run_test(test_set_add_path_hook_resets_hook);
	run_test(test_last_match_is_pre_processed);
	run_test(test_set_add_path_hook_does_not_affect_non_path_functions);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
