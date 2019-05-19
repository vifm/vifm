#include <stic.h>

#include <string.h> /* strcpy() */

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/flist_hist.h"
#include "../../src/flist_pos.h"
#include "../../src/status.h"

#include "utils.h"

SETUP()
{
	view_setup(&lwin);
	setup_grid(&lwin, 1, 1, 1);
	curr_view = &lwin;
	view_setup(&rwin);
	setup_grid(&rwin, 1, 1, 1);
	other_view = &rwin;

	opt_handlers_setup();

	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);
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

	columns_teardown();
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
	tabs_new(NULL, NULL);

	assert_int_equal(2, tabs_visitor_count("/l"));
	assert_int_equal(2, tabs_visitor_count("/r"));
	assert_int_equal(0, tabs_visitor_count("/c"));
}

TEST(invisible_panes_are_accounted_for_on_counting_visitors_with_pane_tabs)
{
	strcpy(lwin.curr_dir, "/l");
	strcpy(rwin.curr_dir, "/r");
	cfg.pane_tabs = 1;
	tabs_new(NULL, NULL);

	assert_int_equal(2, tabs_visitor_count("/l"));
	assert_int_equal(1, tabs_visitor_count("/r"));
	assert_int_equal(0, tabs_visitor_count("/c"));
}

TEST(pane_tabs_are_swapped_on_switch)
{
	cfg.pane_tabs = 1;
	tabs_new(NULL, NULL);
	tabs_new(NULL, NULL);
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
	tabs_new(NULL, NULL);
	tabs_new(NULL, NULL);

	tabs_goto(2);

	assert_true(tabs_get(&lwin, 2, &tab_info));
	assert_true(tab_info.view == &lwin);
}

TEST(goto_tab_checks_boundaries_of_pane_tabs)
{
	tab_info_t tab_info;

	cfg.pane_tabs = 1;
	tabs_new(NULL, NULL);
	tabs_new(NULL, NULL);

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

	tabs_new(NULL, NULL);
	tabs_new(NULL, NULL);

	tabs_goto(2);

	assert_true(tabs_get(&lwin, 2, &tab_info));
	assert_true(tab_info.view == &lwin);
}

TEST(goto_tab_checks_boundaries_of_global_tabs)
{
	tab_info_t tab_info;

	tabs_new(NULL, NULL);
	tabs_new(NULL, NULL);

	tabs_goto(-1);
	assert_true(tabs_get(&lwin, 2, &tab_info));
	assert_true(tab_info.view == &lwin);

	tabs_goto(10);
	assert_true(tabs_get(&lwin, 2, &tab_info));
	assert_true(tab_info.view == &lwin);
}

TEST(view_roles_are_swapped_on_switching_global_tabs)
{
	tabs_new(NULL, NULL);
	tabs_new(NULL, NULL);
	curr_view = &rwin;
	other_view = &lwin;

	tabs_goto(1);

	assert_true(curr_view == &lwin);
	assert_true(other_view == &rwin);
}

TEST(pane_tab_is_closed)
{
	cfg.pane_tabs = 1;
	tabs_new(NULL, NULL);

	tabs_goto(0);
	tabs_close();

	assert_int_equal(1, tabs_count(&lwin));
}

TEST(global_tab_is_closed)
{
	tabs_new(NULL, NULL);

	tabs_goto(0);
	tabs_close();

	assert_int_equal(1, tabs_count(&lwin));
}

TEST(pane_tabs_are_traversed_forward)
{
	tab_info_t tab_info;

	cfg.pane_tabs = 1;
	tabs_new(NULL, NULL);

	tabs_next(1);

	assert_true(tabs_get(&lwin, 0, &tab_info));
	assert_true(tab_info.view == &lwin);
}

TEST(pane_tabs_are_traversed_back)
{
	tab_info_t tab_info;

	cfg.pane_tabs = 1;
	tabs_new(NULL, NULL);

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

	tabs_new(NULL, NULL);
	assert_true(tabs_get(&lwin, 0, &tab_info));
	assert_true(tab_info.view != &lwin);
	assert_true(tab_info.view != &rwin);

	curr_view = &rwin;
	other_view = &lwin;
	tabs_new(NULL, NULL);
	assert_true(tabs_get(&lwin, 1, &tab_info));
	assert_true(tab_info.view != &lwin);
	assert_true(tab_info.view != &rwin);
}

TEST(opening_tab_in_new_location_updates_history)
{
	char cwd[PATH_MAX + 1], sandbox[PATH_MAX + 1], test_data[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", cwd);
	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);

	strcpy(lwin.curr_dir, test_data);
	assert_success(populate_dir_list(&lwin, 0));

	lwin.list_pos = fpos_find_by_name(&lwin, "compare");

	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(5);
	cfg_resize_histories(0);
	cfg_resize_histories(5);
	curr_stats.load_stage = 2;

	lwin.list_pos = fpos_find_by_name(&lwin, "rename");

	cfg.pane_tabs = 1;
	assert_int_equal(0, lwin.history_pos);
	assert_string_equal(test_data, lwin.history[0].dir);
	assert_string_equal("compare", lwin.history[0].file);
	tabs_new(NULL, sandbox);
	assert_int_equal(1, lwin.history_pos);
	assert_string_equal(test_data, lwin.history[0].dir);
	assert_string_equal("rename", lwin.history[0].file);

	tab_info_t tab_info;
	assert_true(tabs_get(&lwin, 0, &tab_info));
	assert_int_equal(0, tab_info.view->history_pos);
	assert_string_equal(test_data, tab_info.view->history[0].dir);
	assert_string_equal("compare", tab_info.view->history[0].file);
	assert_true(tabs_get(&lwin, 1, &tab_info));
	assert_int_equal(1, tab_info.view->history_pos);
	assert_string_equal(test_data, tab_info.view->history[0].dir);
	assert_string_equal("rename", tab_info.view->history[0].file);

	curr_stats.load_stage = 0;
	cfg_resize_histories(0);
}

TEST(opening_tab_in_new_location_fetches_position_from_history)
{
	char cwd[PATH_MAX + 1], sandbox[PATH_MAX + 1], test_data[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", cwd);
	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);

	strcpy(lwin.curr_dir, sandbox);
	assert_success(populate_dir_list(&lwin, 0));

	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(5);
	cfg_resize_histories(0);
	cfg_resize_histories(5);
	curr_stats.load_stage = 2;

	cfg.pane_tabs = 1;
	tabs_new(NULL, test_data);
	assert_int_equal(1, lwin.history_pos);
	assert_string_equal(test_data, lwin.history[1].dir);

	curr_stats.load_stage = 0;
	cfg_resize_histories(0);
}

TEST(opening_tab_in_new_location_records_new_location_in_history)
{
	char cwd[PATH_MAX + 1], sandbox[PATH_MAX + 1], test_data[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", cwd);
	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);

	strcpy(lwin.curr_dir, sandbox);
	assert_success(populate_dir_list(&lwin, 0));

	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(5);
	cfg_resize_histories(0);
	cfg_resize_histories(5);
	curr_stats.load_stage = 2;
	curr_stats.ch_pos = 1;

	flist_hist_save(&lwin, test_data, "rename", 0);
	flist_hist_save(&lwin, sandbox, "..", 0);

	cfg.pane_tabs = 1;
	tabs_new(NULL, test_data);
	assert_string_equal("rename", get_current_file_name(&lwin));

	tab_info_t tab_info;
	assert_true(tabs_get(&lwin, 0, &tab_info));
	assert_string_equal("..", get_current_file_name(tab_info.view));
	assert_true(tabs_get(&lwin, 1, &tab_info));
	assert_string_equal("rename", get_current_file_name(tab_info.view));

	curr_stats.ch_pos = 0;
	curr_stats.load_stage = 0;
	cfg_resize_histories(0);
}

TEST(reloading_propagates_values_of_current_views_with_global_tabs)
{
	cfg.pane_tabs = 0;

	lwin.miller_view = 1;
	rwin.miller_view = 0;
	tabs_new(NULL, NULL);
	lwin.miller_view = 0;
	rwin.miller_view = 1;

	tabs_reload();

	tabs_goto(0);
	assert_false(lwin.miller_view);
	assert_true(rwin.miller_view);

	tabs_goto(1);
	assert_false(lwin.miller_view);
	assert_true(rwin.miller_view);
}

TEST(reloading_propagates_values_of_current_views_with_pane_tabs)
{
	cfg.pane_tabs = 1;

	lwin.miller_view = 1;
	tabs_new(NULL, NULL);
	lwin.miller_view = 0;

	curr_view = &rwin;
	other_view = &lwin;

	rwin.miller_view = 0;
	tabs_new(NULL, NULL);
	rwin.miller_view = 1;

	tabs_reload();

	tab_info_t tab_info;

	assert_true(tabs_get(&lwin, 0, &tab_info));
	assert_false(tab_info.view->miller_view);
	assert_true(tabs_get(&lwin, 1, &tab_info));
	assert_false(tab_info.view->miller_view);

	assert_true(tabs_get(&rwin, 0, &tab_info));
	assert_true(tab_info.view->miller_view);
	assert_true(tabs_get(&rwin, 1, &tab_info));
	assert_true(tab_info.view->miller_view);
}

TEST(tabs_enum_ignores_active_pane_for_global_tabs)
{
	tabs_new(NULL, NULL);

	tab_info_t ltab_info, rtab_info;
	assert_true(tabs_enum(&lwin, 0, &ltab_info));
	assert_true(tabs_enum(&rwin, 0, &rtab_info));

	assert_true(ltab_info.view != rtab_info.view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
