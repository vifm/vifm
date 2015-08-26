#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */

#include "../../src/engine/completion.h"
#include "../../src/bmarks.h"

TEST(completes_single_match)
{
	char *completed;

	assert_success(bmarks_set("fake/dir", "atag,btag,ctag"));

	vle_compl_reset();

	bmarks_complete(0, NULL, "a");

	completed = vle_compl_next();
	assert_string_equal("atag", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("atag", completed);
	free(completed);
}

TEST(completes_multiple_matches)
{
	char *completed;

	assert_success(bmarks_set("fake/dir", "atag,aatag,ctag"));

	vle_compl_reset();

	bmarks_complete(0, NULL, "a");

	completed = vle_compl_next();
	assert_string_equal("aatag", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("atag", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("a", completed);
	free(completed);
}

TEST(skips_already_typed_tags)
{
	char *completed;
	const char *used[] = {"atag"};

	assert_success(bmarks_set("fake/dir", "atag,aatag,ctag"));

	vle_compl_reset();

	bmarks_complete(1, (char **)used, "a");

	completed = vle_compl_next();
	assert_string_equal("aatag", completed);
	free(completed);

	completed = vle_compl_next();
	assert_string_equal("aatag", completed);
	free(completed);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
