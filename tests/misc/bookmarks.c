#include "seatest.h"

#include "../../src/bookmarks.h"
#include "../../src/ui.h"

static void
test_unexistant_bookmark(void)
{
	assert_int_equal(1, move_to_bookmark(&lwin, 'b'));
}

void
bookmarks_tests(void)
{
	test_fixture_start();

	run_test(test_unexistant_bookmark);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
