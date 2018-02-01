#include <stic.h>

#include <string.h> /* strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"

#include "utils.h"

static void format_none(int id, const void *data, size_t buf_len, char buf[]);

SETUP()
{
	view_setup(&lwin);
	setup_grid(&lwin, 1, 1, 1);
	curr_view = &lwin;
	view_setup(&rwin);
	setup_grid(&rwin, 1, 1, 1);
	other_view = &rwin;

	opt_handlers_setup();

	columns_add_column_desc(SK_BY_NAME, &format_none);
	columns_add_column_desc(SK_BY_SIZE, &format_none);
}

static void
format_none(int id, const void *data, size_t buf_len, char buf[])
{
	buf[0] = '\0';
}

TEARDOWN()
{
	tabs_only(&lwin);
	tabs_only(&rwin);
	cfg.pane_tabs = 0;
	tabs_only(&lwin);

	opt_handlers_teardown();

	view_teardown(&lwin);
	view_teardown(&rwin);

	columns_clear_column_descs();
}

TEST(visible_panes_are_accounted_for_on_counting_visitors)
{
	strcpy(lwin.curr_dir, "/a");
	strcpy(rwin.curr_dir, "/a");

	assert_int_equal(2, tabs_visitor_count("/a"));
	assert_int_equal(0, tabs_visitor_count("/b"));
}

TEST(invisible_panes_are_accounted_for_on_counting_visitors_with_global_tabs)
{
	strcpy(lwin.curr_dir, "/l");
	strcpy(rwin.curr_dir, "/r");
	tabs_new(NULL);

	assert_int_equal(2, tabs_visitor_count("/l"));
	assert_int_equal(2, tabs_visitor_count("/r"));
	assert_int_equal(0, tabs_visitor_count("/c"));
}

TEST(invisible_panes_are_accounted_for_on_counting_visitors_with_pane_tabs)
{
	strcpy(lwin.curr_dir, "/l");
	strcpy(rwin.curr_dir, "/r");
	cfg.pane_tabs = 1;
	tabs_new(NULL);

	assert_int_equal(2, tabs_visitor_count("/l"));
	assert_int_equal(1, tabs_visitor_count("/r"));
	assert_int_equal(0, tabs_visitor_count("/c"));
}

TEST(pane_tabs_are_swapped_on_switch)
{
	strcpy(lwin.curr_dir, "/l");
	strcpy(rwin.curr_dir, "/r");
	cfg.pane_tabs = 1;
	tabs_new(NULL);
	tabs_new(NULL);
	curr_view = &rwin;
	other_view = &lwin;

	tabs_switch_panes();

	assert_int_equal(1, tabs_count(&lwin));
	assert_int_equal(3, tabs_count(&rwin));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
