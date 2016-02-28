#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */

#include "../../src/engine/completion.h"
#include "../../src/utils/str.h"

static char *
add_dquotes(const char match[])
{
	return format_str("\"%s\"", match);
}

TEST(set_add_path_hook_sets_hook)
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

TEST(set_add_path_hook_resets_hook)
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

TEST(last_match_is_pre_processed)
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

TEST(set_add_path_hook_does_not_affect_non_path_functions)
{
	char *match;

	vle_compl_set_add_path_hook(&add_dquotes);
	assert_int_equal(0, vle_compl_add_match("a b", ""));
	assert_int_equal(0, vle_compl_add_match("a b c", ""));
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
