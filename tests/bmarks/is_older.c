#include <stic.h>

#include <stddef.h> /* NULL */
#include <time.h> /* time() */

#include "../../src/bmarks.h"

TEST(check_for_newer)
{
	assert_success(bmarks_setup("newer", "tag", 0U));
	assert_true(bmark_is_older("newer", time(NULL)));
}

TEST(check_for_older)
{
	assert_success(bmarks_set("newer", "tag"));
	assert_false(bmark_is_older("newer", 0));
}

TEST(path_is_canonicalized)
{
	assert_success(bmarks_set("newer/.", "tag"));
	assert_false(bmark_is_older("newer", 0));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
