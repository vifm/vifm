#include <stic.h>

#include "../../src/bmarks.h"

#include "utils.h"

TEST(empty_tag_is_ignored)
{
	assert_success(bmarks_set("empty/tag/is/ignored", ""));
	assert_string_equal(NULL, get_tags("empty/tag/is/ignored"));
}

TEST(single_tag_is_set)
{
	assert_success(bmarks_set("single/tag/is/set", "tag"));
	assert_string_equal("tag", get_tags("single/tag/is/set"));
}

TEST(tag_is_overwritten)
{
	assert_success(bmarks_set("tag/is/overwritten", "old-tag"));
	assert_success(bmarks_set("tag/is/overwritten", "new-tag"));
	assert_string_equal("new-tag", get_tags("tag/is/overwritten"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
