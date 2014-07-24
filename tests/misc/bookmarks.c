#include "seatest.h"

#include <stddef.h> /* NULL */
#include <string.h> /* strlen() */

#include "../../src/bookmarks.h"
#include "../../src/ui.h"

static void
test_unexistant_bookmark(void)
{
	assert_int_equal(1, goto_bookmark(&lwin, 'b'));
}

static void
test_all_valid_bookmarks_can_be_queried(void)
{
	const int bookmark_count = strlen(valid_bookmarks);
	int i;
	for(i = 0; i < bookmark_count; ++i)
	{
		assert_true(get_bookmark(i) != NULL);
	}
}

void
bookmarks_tests(void)
{
	test_fixture_start();

	run_test(test_unexistant_bookmark);
	run_test(test_all_valid_bookmarks_can_be_queried);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
