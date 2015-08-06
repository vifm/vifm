#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/abbrevs.h"
#include "../../src/engine/completion.h"

SETUP()
{
	assert_success(vle_abbr_add(L"lhs1", L"rhs1"));
	assert_success(vle_abbr_add(L"lhs2", L"rhs2"));
	assert_success(vle_abbr_add(L"seq", L"rhs3"));
}

TEST(nothing_to_complete)
{
	char *completed;

	vle_abbr_reset();

	vle_compl_reset();
	vle_abbr_complete("");
	completed = vle_compl_next();
	assert_string_equal("", completed);
	free(completed);
}

TEST(ambiguous)
{
	char *completed;

	vle_compl_reset();
	vle_abbr_complete("l");

	completed = vle_compl_next();
	assert_string_equal("lhs1", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("lhs2", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("l", completed);
	free(completed);
}

TEST(non_ambiguous)
{
	char *completed;

	vle_compl_reset();
	vle_abbr_complete("s");

	completed = vle_compl_next();
	assert_string_equal("seq", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("seq", completed);
	free(completed);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
