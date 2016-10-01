#include <stic.h>

#include <string.h> /* strcpy() */

#include "../../src/ui/ui.h"
#include "../../src/flist_pos.h"
#include "../../src/compare.h"

#include "utils.h"

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);

	opt_handlers_setup();
}

TEARDOWN()
{
	view_teardown(&lwin);

	opt_handlers_teardown();
}

TEST(compare_view_defines_id_grouping)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, 0);

	assert_int_equal(3, lwin.list_rows);

	assert_int_equal(0, lwin.list_pos);
	assert_int_equal(1, lwin.list_pos = flist_find_group(&lwin, 1));
	assert_int_equal(2, lwin.list_pos = flist_find_group(&lwin, 1));
	assert_int_equal(1, lwin.list_pos = flist_find_group(&lwin, 0));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
