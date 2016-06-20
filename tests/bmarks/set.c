#include <stic.h>

#include "../../src/bmarks.h"

#include "utils.h"

TEST(lonely_empty_tag_is_rejected)
{
	assert_failure(bmarks_set("lonely/empty/tag/is/rejected", ""));
	assert_string_equal(NULL, get_tags("lonely/empty/tag/is/rejected"));
}

TEST(empty_tag_is_rejected_in_any_form)
{
	assert_failure(bmarks_set("empty/tag/is/rejected", ","));
	assert_string_equal(NULL, get_tags("empty/tag/is/rejected"));

	assert_failure(bmarks_set("empty/tag/is/rejected", ",,"));
	assert_string_equal(NULL, get_tags("empty/tag/is/rejected"));

	assert_failure(bmarks_set("empty/tag/is/rejected", "abc,,"));
	assert_string_equal(NULL, get_tags("empty/tag/is/rejected"));

	assert_failure(bmarks_set("empty/tag/is/rejected", ",xz,de"));
	assert_string_equal(NULL, get_tags("empty/tag/is/rejected"));

	assert_failure(bmarks_set("empty/tag/is/rejected", "abc,,de"));
	assert_string_equal(NULL, get_tags("empty/tag/is/rejected"));
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

TEST(path_is_canonicalized)
{
	assert_success(bmarks_set("parent/../dir", "tag"));
	assert_string_equal("tag", get_tags("dir"));
}

TEST(canonicalization_preserves_trailing_slash)
{
	assert_success(bmarks_set("parent/../dir/", "tag"));
	assert_string_equal("tag", get_tags("dir/"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
