#include <stic.h>

#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/matcher.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/flist_hist.h"
#include "../../src/flist_pos.h"
#include "../../src/marks.h"
#include "../../src/opt_handlers.h"
#include "../../src/status.h"

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

TEST(new_global_tab_is_inserted_after_current)
{
	tabs_new(NULL, NULL);
	tabs_goto(0);
	tabs_new(NULL, NULL);
	assert_int_equal(1, tabs_current(&lwin));
}

TEST(new_pane_tab_is_inserted_after_current)
{
	cfg.pane_tabs = 1;
	tabs_new(NULL, NULL);
	tabs_goto(0);
	tabs_new(NULL, NULL);
	assert_int_equal(1, tabs_current(&lwin));
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

	flist_hist_setup(&lwin, test_data, "rename", 0, 1);
	flist_hist_setup(&lwin, sandbox, "..", 0, 1);

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

TEST(tabs_enum_ignores_active_pane_for_global_tabs)
{
	tabs_new(NULL, NULL);

	tab_info_t ltab_info, rtab_info;
	assert_true(tabs_enum(&lwin, 0, &ltab_info));
	assert_true(tabs_enum(&rwin, 0, &rtab_info));

	assert_true(ltab_info.view != rtab_info.view);
}

TEST(tabs_enum_all_lists_all_global_tabs)
{
	tabs_new(NULL, NULL);

	tab_info_t tab_info1, tab_info2;

	assert_true(tabs_enum_all(0, &tab_info1));
	assert_true(tabs_enum(&lwin, 0, &tab_info2));
	assert_true(tab_info1.view == tab_info2.view);

	assert_true(tabs_enum_all(1, &tab_info1));
	assert_true(tabs_enum(&lwin, 1, &tab_info2));
	assert_true(tab_info1.view == tab_info2.view);

	assert_true(tabs_enum_all(2, &tab_info1));
	assert_true(tabs_enum(&rwin, 0, &tab_info2));
	assert_true(tab_info1.view == tab_info2.view);

	assert_true(tabs_enum_all(3, &tab_info1));
	assert_true(tabs_enum(&rwin, 1, &tab_info2));
	assert_true(tab_info1.view == tab_info2.view);
}

TEST(tabs_enum_all_lists_all_pane_tabs)
{
	cfg.pane_tabs = 1;
	tabs_new(NULL, NULL);
	tabs_new(NULL, NULL);

	tab_info_t tab_info1, tab_info2;

	assert_true(tabs_enum_all(0, &tab_info1));
	assert_true(tabs_enum(&lwin, 0, &tab_info2));
	assert_true(tab_info1.view == tab_info2.view);

	assert_true(tabs_enum_all(1, &tab_info1));
	assert_true(tabs_enum(&lwin, 1, &tab_info2));
	assert_true(tab_info1.view == tab_info2.view);

	assert_true(tabs_enum_all(2, &tab_info1));
	assert_true(tabs_enum(&lwin, 2, &tab_info2));
	assert_true(tab_info1.view == tab_info2.view);

	assert_true(tabs_enum_all(3, &tab_info1));
	assert_true(tabs_enum(&rwin, 0, &tab_info2));
	assert_true(tab_info1.view == tab_info2.view);
}

TEST(new_global_tabs_are_appended_on_setup)
{
	tab_layout_t layout;
	tabs_layout_fill(&layout);

	view_t *left, *right;
	assert_success(tabs_setup_gtab("1", &layout, &left, &right));
	assert_success(tabs_setup_gtab("2", &layout, &left, &right));

	tab_info_t tab_info;
	assert_true(tabs_enum(&lwin, 0, &tab_info));
	assert_string_equal(NULL, tab_info.name);
	assert_true(tabs_enum(&lwin, 1, &tab_info));
	assert_string_equal("1", tab_info.name);
	assert_true(tabs_enum(&lwin, 2, &tab_info));
	assert_string_equal("2", tab_info.name);
}

TEST(new_pane_tabs_are_appended_on_setup)
{
	cfg.pane_tabs = 1;
	assert_non_null(tabs_setup_ptab(&lwin, "1", 0));
	assert_non_null(tabs_setup_ptab(&lwin, "2", 0));

	tab_info_t tab_info;
	assert_true(tabs_enum(&lwin, 0, &tab_info));
	assert_string_equal(NULL, tab_info.name);
	assert_true(tabs_enum(&lwin, 1, &tab_info));
	assert_string_equal("1", tab_info.name);
	assert_true(tabs_enum(&lwin, 2, &tab_info));
	assert_string_equal("2", tab_info.name);
}

TEST(newly_setup_global_tabs_have_empty_history)
{
	tab_layout_t layout;
	tabs_layout_fill(&layout);

	view_t *left, *right;
	assert_success(tabs_setup_gtab("1", &layout, &left, &right));
	assert_int_equal(0, left->history_num);
	assert_int_equal(0, right->history_num);
}

TEST(newly_setup_pane_tabs_have_empty_history)
{
	cfg.pane_tabs = 1;
	view_t *view = tabs_setup_ptab(&lwin, "1", 0);
	assert_non_null(view);
	assert_int_equal(0, view->history_num);
}

TEST(layout_of_global_tab_is_applied)
{
	tab_layout_t layout = {
		.active_pane = 0,
		.only_mode = 1,
		.split = HSPLIT,
		.splitter_pos = -1,
		.preview = 0,
	};

	view_t *left, *right;
	assert_success(tabs_setup_gtab("1", &layout, &left, &right));

	curr_stats.number_of_windows = 2;
	curr_stats.split = VSPLIT;

	tabs_goto(1);

	assert_int_equal(1, curr_stats.number_of_windows);
	assert_int_equal(HSPLIT, curr_stats.split);
}

TEST(layout_of_pane_tab_is_applied)
{
	cfg.pane_tabs = 1;
	assert_non_null(tabs_setup_ptab(&lwin, "1", 1));

	curr_stats.preview.on = 0;

	tabs_goto(1);

	assert_true(curr_stats.preview.on);
}

TEST(layout_of_global_tab_is_returned)
{
	tab_layout_t layout = {
		.active_pane = 0,
		.only_mode = 1,
		.split = HSPLIT,
		.splitter_pos = -1,
		.preview = 0,
	};

	view_t *left, *right;
	assert_success(tabs_setup_gtab("1", &layout, &left, &right));

	tab_info_t tab_info;
	assert_true(tabs_get(&lwin, 1, &tab_info));
	assert_true(tab_info.layout.only_mode);
	assert_int_equal(HSPLIT, tab_info.layout.split);
}

TEST(layout_of_pane_tab_is_returned)
{
	cfg.pane_tabs = 1;
	assert_non_null(tabs_setup_ptab(&lwin, "1", 1));

	tab_info_t tab_info;
	assert_true(tabs_get(&lwin, 1, &tab_info));
	assert_true(tab_info.layout.preview);
}

TEST(global_local_options_and_tabs)
{
	curr_stats.global_local_settings = 1;

	lwin.hide_dot_g = lwin.hide_dot = 0;
	rwin.hide_dot_g = rwin.hide_dot = 0;

	tabs_new(NULL, NULL);
	assert_success(process_set_args("nodotfiles", 1, 1));

	int i;
	tab_info_t tab_info;
	for(i = 0; tabs_enum_all(i, &tab_info); ++i)
	{
		assert_true(tab_info.view->hide_dot_g);
		assert_true(tab_info.view->hide_dot);
	}

	curr_stats.global_local_settings = 0;
}

TEST(global_local_dotfilter_and_tabs)
{
	curr_stats.global_local_settings = 1;
	init_modes();
	init_commands();

	lwin.hide_dot_g = lwin.hide_dot = 1;
	rwin.hide_dot_g = rwin.hide_dot = 1;

	tabs_new(NULL, NULL);

	int i;
	tab_info_t tab_info;

	assert_success(exec_commands("normal zo", &lwin, CIT_COMMAND));
	for(i = 0; tabs_enum_all(i, &tab_info); ++i)
	{
		assert_false(tab_info.view->hide_dot_g);
		assert_false(tab_info.view->hide_dot);
	}

	assert_success(exec_commands("normal za", &lwin, CIT_COMMAND));
	for(i = 0; tabs_enum_all(i, &tab_info); ++i)
	{
		assert_true(tab_info.view->hide_dot_g);
		assert_true(tab_info.view->hide_dot);
	}

	vle_keys_reset();
	vle_cmds_reset();
	curr_stats.global_local_settings = 0;
}

TEST(global_local_manualfilter_and_tabs)
{
	curr_stats.global_local_settings = 1;
	init_modes();
	init_commands();

	tabs_new(NULL, NULL);
	assert_success(exec_commands("filter /y/", &lwin, CIT_COMMAND));

	int i;
	tab_info_t tab_info;
	for(i = 0; tabs_enum_all(i, &tab_info); ++i)
	{
		assert_string_equal("/y/", matcher_get_expr(tab_info.view->manual_filter));
	}

	vle_keys_reset();
	vle_cmds_reset();
	curr_stats.global_local_settings = 0;
}

TEST(local_options_are_reset_if_path_is_not_changing)
{
	assert_success(process_set_args("dotfiles", 1, 0));
	assert_success(process_set_args("nodotfiles", 0, 1));

	tabs_new(NULL, NULL);

	assert_false(lwin.hide_dot_g);
	assert_false(lwin.hide_dot);
}

TEST(local_options_are_reset_if_path_is_changing)
{
	assert_success(process_set_args("dotfiles", 1, 0));
	assert_success(process_set_args("nodotfiles", 0, 1));

	tabs_new(NULL, SANDBOX_PATH);

	assert_false(lwin.hide_dot_g);
	assert_false(lwin.hide_dot);
}

TEST(direnter_is_called_for_new_tab)
{
	curr_stats.load_stage = -1;
	init_modes();
	init_commands();

	assert_success(process_set_args("dotfiles", 1, 1));

	assert_success(exec_commands("autocmd DirEnter * setlocal nodotfiles", &lwin,
				CIT_COMMAND));

	tabs_new(NULL, SANDBOX_PATH);

	assert_false(lwin.hide_dot_g);
	assert_true(lwin.hide_dot);

	assert_success(exec_commands("autocmd!", &lwin, CIT_COMMAND));

	vle_keys_reset();
	vle_cmds_reset();
	curr_stats.load_stage = 0;
}

TEST(special_marks_are_not_shared_among_tabs)
{
	marks_set_special(curr_view, '<', "l0path", "l0file<");
	tabs_new(NULL, SANDBOX_PATH);
	marks_set_special(curr_view, '<', "l1path", "l1file<");

	const mark_t *mark;

	tabs_goto(0);
	mark = get_mark_by_name(curr_view, '<');
	assert_string_equal("l0path", mark->directory);
	assert_string_equal("l0file<", mark->file);

	tabs_goto(1);
	mark = get_mark_by_name(curr_view, '<');
	assert_string_equal("l1path", mark->directory);
	assert_string_equal("l1file<", mark->file);
}

TEST(tab_ids_global_tabs)
{
	tabs_new(NULL, NULL);
	tabs_new(NULL, NULL);
	tabs_new(NULL, NULL);

	tab_info_t tab_info;
	assert_true(tabs_get(&lwin, 1, &tab_info));
	int id = tab_info.id;

	/* Multiplied by 3 because each new global tab creates two pane tabs. */
	assert_true(tabs_get(&lwin, 2, &tab_info));
	assert_int_equal(id + 1*3, tab_info.id);
	assert_true(tabs_get(&lwin, 3, &tab_info));
	assert_int_equal(id + 2*3, tab_info.id);
}

TEST(tab_ids_pane_tabs)
{
	cfg.pane_tabs = 1;

	tabs_new(NULL, NULL);
	tabs_new(NULL, NULL);
	tabs_new(NULL, NULL);

	tab_info_t tab_info;
	assert_true(tabs_get(&lwin, 1, &tab_info));
	int id = tab_info.id;

	assert_true(tabs_get(&lwin, 2, &tab_info));
	assert_int_equal(id + 1, tab_info.id);
	assert_true(tabs_get(&lwin, 3, &tab_info));
	assert_int_equal(id + 2, tab_info.id);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
