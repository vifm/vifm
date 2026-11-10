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
	vle_compl_set_add_path_hook(&add_dquotes);

	assert_int_equal(0, vle_compl_add_path_match("a b c"));
	assert_int_equal(0, vle_compl_add_path_match("a b"));
	assert_int_equal(0, vle_compl_add_path_match("a"));
	vle_compl_finish_group();
	assert_int_equal(3, vle_compl_get_count());

	char *match;
	assert_string_equal("\"a b c\"", match = vle_compl_next());
	free(match);
	assert_string_equal("\"a b\"", match = vle_compl_next());
	free(match);
	assert_string_equal("\"a\"", match = vle_compl_next());
	free(match);

	vle_compl_set_add_path_hook(NULL);
}

TEST(set_add_path_hook_resets_hook)
{
	vle_compl_set_add_path_hook(&add_dquotes);
	assert_int_equal(0, vle_compl_add_path_match("a b"));
	assert_int_equal(0, vle_compl_add_path_match("x y"));

	vle_compl_set_add_path_hook(NULL);
	assert_int_equal(0, vle_compl_add_path_match("a b c"));

	vle_compl_finish_group();
	assert_int_equal(3, vle_compl_get_count());

	char *match;
	assert_string_equal("\"a b\"", match = vle_compl_next());
	free(match);
	assert_string_equal("\"x y\"", match = vle_compl_next());
	free(match);
	assert_string_equal("a b c", match = vle_compl_next());
	free(match);
}

TEST(last_match_is_pre_processed)
{
	vle_compl_set_add_path_hook(&add_dquotes);
	assert_int_equal(0, vle_compl_add_path_match("a b"));
	assert_int_equal(0, vle_compl_add_path_match("a b c"));
	vle_compl_finish_group();
	assert_int_equal(0, vle_compl_add_last_path_match("a"));

	assert_int_equal(3, vle_compl_get_count());

	char *match;
	assert_string_equal("\"a b c\"", match = vle_compl_next());
	free(match);
	assert_string_equal("\"a b\"", match = vle_compl_next());
	free(match);
	assert_string_equal("\"a\"", match = vle_compl_next());
	free(match);
}

TEST(set_add_path_hook_does_not_affect_non_path_functions)
{
	vle_compl_set_add_path_hook(&add_dquotes);
	assert_int_equal(0, vle_compl_add_match("a b", ""));
	assert_int_equal(0, vle_compl_add_match("a b c", ""));
	vle_compl_finish_group();
	assert_int_equal(0, vle_compl_add_last_match("a"));

	assert_int_equal(3, vle_compl_get_count());

	char *match;
	assert_string_equal("a b", match = vle_compl_next());
	free(match);
	assert_string_equal("a b c", match = vle_compl_next());
	free(match);
	assert_string_equal("a", match = vle_compl_next());
	free(match);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
