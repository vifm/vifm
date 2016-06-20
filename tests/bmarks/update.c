#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/bmarks.h"

#include "utils.h"

TEST(bmark_is_renamed)
{
	assert_success(bmarks_setup("old", "tag", 0U));

	bmarks_file_moved("old", "new");

	assert_string_equal("tag", get_tags("new"));
	assert_string_equal(NULL, get_tags("old"));
}

TEST(path_is_canonicalized)
{
	assert_success(bmarks_setup("old", "tag", 0U));

	bmarks_file_moved("old/.", "new");

	assert_string_equal("tag", get_tags("new"));
	assert_string_equal(NULL, get_tags("old"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
