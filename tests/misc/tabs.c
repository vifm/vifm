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
	cfg.pane_tabs = 1;
	tabs_new(NULL);
	tabs_new(NULL);
	curr_view = &rwin;
	other_view = &lwin;

	tabs_switch_panes();

	assert_int_equal(1, tabs_count(&lwin));
	assert_int_equal(3, tabs_count(&rwin));
}

TEST(goto_tab_stays_on_current_pane_tab)
{
	tab_info_t tab_info;

	cfg.pane_tabs = 1;
	tabs_new(NULL);
	tabs_new(NULL);

	tabs_goto(2);

	assert_true(tabs_get(&lwin, 2, &tab_info));
	assert_true(tab_info.view == &lwin);
}

TEST(goto_tab_checks_boundaries_of_pane_tabs)
{
	tab_info_t tab_info;

	cfg.pane_tabs = 1;
	tabs_new(NULL);
	tabs_new(NULL);

	tabs_goto(-1);
	assert_true(tabs_get(&lwin, 2, &tab_info));
	assert_true(tab_info.view == &lwin);

	tabs_goto(10);
	assert_true(tabs_get(&lwin, 2, &tab_info));
	assert_true(tab_info.view == &lwin);
}

TEST(goto_tab_stays_on_current_global_tab)
{
	tab_info_t tab_info;

	tabs_new(NULL);
	tabs_new(NULL);

	tabs_goto(2);

	assert_true(tabs_get(&lwin, 2, &tab_info));
	assert_true(tab_info.view == &lwin);
}

TEST(goto_tab_checks_boundaries_of_global_tabs)
{
	tab_info_t tab_info;

	tabs_new(NULL);
	tabs_new(NULL);

	tabs_goto(-1);
	assert_true(tabs_get(&lwin, 2, &tab_info));
	assert_true(tab_info.view == &lwin);

	tabs_goto(10);
	assert_true(tabs_get(&lwin, 2, &tab_info));
	assert_true(tab_info.view == &lwin);
}

TEST(view_roles_are_swapped_on_switching_global_tabs)
{
	tabs_new(NULL);
	tabs_new(NULL);
	curr_view = &rwin;
	other_view = &lwin;

	tabs_goto(1);

	assert_true(curr_view == &lwin);
	assert_true(other_view == &rwin);
}

TEST(pane_tab_is_closed)
{
	cfg.pane_tabs = 1;
	tabs_new(NULL);

	tabs_goto(0);
	tabs_close();

	assert_int_equal(1, tabs_count(&lwin));
}

TEST(global_tab_is_closed)
{
	tabs_new(NULL);

	tabs_goto(0);
	tabs_close();

	assert_int_equal(1, tabs_count(&lwin));
}

TEST(pane_tabs_are_traversed_forward)
{
	tab_info_t tab_info;

	cfg.pane_tabs = 1;
	tabs_new(NULL);

	tabs_next(1);

	assert_true(tabs_get(&lwin, 0, &tab_info));
	assert_true(tab_info.view == &lwin);
}

TEST(pane_tabs_are_traversed_back)
{
	tab_info_t tab_info;

	cfg.pane_tabs = 1;
	tabs_new(NULL);

	tabs_previous(1);

	assert_true(tabs_get(&lwin, 0, &tab_info));
	assert_true(tab_info.view == &lwin);
}

TEST(tabs_get_checks_index)
{
	tab_info_t tab_info;

	cfg.pane_tabs = 0;
	assert_false(tabs_get(&lwin, -1, &tab_info));
	assert_false(tabs_get(&lwin, 1, &tab_info));

	cfg.pane_tabs = 1;
	assert_false(tabs_get(&lwin, -1, &tab_info));
	assert_false(tabs_get(&lwin, 1, &tab_info));
}

TEST(tabs_get_returns_inactive_global_tab)
{
	tab_info_t tab_info;

	tabs_new(NULL);
	assert_true(tabs_get(&lwin, 0, &tab_info));
	assert_true(tab_info.view != &lwin);
	assert_true(tab_info.view != &rwin);

	curr_view = &rwin;
	other_view = &lwin;
	tabs_new(NULL);
	assert_true(tabs_get(&lwin, 1, &tab_info));
	assert_true(tab_info.view != &lwin);
	assert_true(tab_info.view != &rwin);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
