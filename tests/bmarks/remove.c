#include <stic.h>

#include "../../src/bmarks.h"

#include "utils.h"

TEST(inexistent_bookmark_is_removed)
{
	bmarks_remove("no/bookmark");
	assert_string_equal(NULL, get_tags("no/bookmark"));
}

TEST(existent_bookmark_is_removed)
{
	assert_success(bmarks_set("bookmark", "tag"));
	bmarks_remove("bookmark");
	assert_string_equal(NULL, get_tags("bookmark"));
}

TEST(other_bookmark_is_not_touched)
{
	assert_success(bmarks_set("bookmark1", "tag1"));
	assert_success(bmarks_set("bookmark2", "tag2"));
	bmarks_remove("bookmark1");
	assert_string_equal("tag2", get_tags("bookmark2"));
}

TEST(bookmark_can_be_recreated)
{
	assert_success(bmarks_set("bookmark", "tag"));
	bmarks_remove("bookmark");
	assert_success(bmarks_set("bookmark", "tag"));
	assert_string_equal("tag", get_tags("bookmark"));
}

TEST(path_is_canonicalized)
{
	assert_success(bmarks_set("parent/../dir", "tag"));
	bmarks_remove("dir");
	assert_string_equal(NULL, get_tags("dir"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
