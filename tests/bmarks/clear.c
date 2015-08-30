#include <stic.h>

#include <stddef.h> /* NULL */

#include "../../src/bmarks.h"

#include "utils.h"

TEST(clear_removes_all_bookmarks)
{
	assert_success(bmarks_set("clear/removes/all/bookmarks", "tag"));
	assert_string_equal("tag", get_tags("clear/removes/all/bookmarks"));
	bmarks_clear();
	assert_string_equal(NULL, get_tags("clear/removes/all/bookmarks"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
