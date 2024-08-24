#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() */
#include <string.h> /* strcpy() */

#include <stubs.h>
#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/view.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/color_scheme.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/cmd_core.h"
#include "../../src/compare.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

SETUP()
{
	view_setup(&lwin);
	setup_grid(&lwin, /*column_count=*/1, /*list_rows=*/1, /*init=*/1);
	curr_view = &lwin;
	view_setup(&rwin);
	setup_grid(&rwin, /*column_count=*/1, /*list_rows=*/1, /*init=*/1);
	other_view = &rwin;

	modes_init();

	opt_handlers_setup();

	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);

	cmds_init();
}

TEARDOWN()
{
	vle_cmds_reset();

	tabs_only(&lwin);
	tabs_only(&rwin);
	cfg.pane_tabs = 0;
	tabs_only(&lwin);

	vle_keys_reset();

	opt_handlers_teardown();

	view_teardown(&lwin);
	view_teardown(&rwin);

	columns_teardown();
}

TEST(tab_is_created_without_name)
{
	tab_info_t tab_info;

	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	assert_int_equal(2, tabs_count(&lwin));

	assert_true(tabs_get(&lwin, 1, &tab_info));
	assert_string_equal(NULL, tab_info.name);
}

TEST(tab_is_not_created_on_wrong_path)
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", cwd);

	(void)cmds_dispatch("tabnew no-such-subdir", &lwin, CIT_COMMAND);
	assert_int_equal(1, tabs_count(&lwin));

	tab_info_t tab_info;
	assert_true(tabs_get(&lwin, 0, &tab_info));
	assert_true(paths_are_same(lwin.curr_dir, TEST_DATA_PATH));
}

TEST(tab_in_path_is_created)
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	char test_data[PATH_MAX + 1];
	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);

	strcpy(lwin.curr_dir, test_data);

	assert_success(cmds_dispatch("tabnew read", &lwin, CIT_COMMAND));
	assert_int_equal(2, tabs_count(&lwin));

	tab_info_t tab_info;
	assert_true(tabs_get(&lwin, 1, &tab_info));

	char read_data[PATH_MAX + 1];
	snprintf(read_data, sizeof(read_data), "%s/read", test_data);
	assert_true(paths_are_same(lwin.curr_dir, read_data));
}

TEST(tab_in_parent_is_created)
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	char test_data[PATH_MAX + 1];
	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);

	snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/read", test_data);

	assert_success(cmds_dispatch("tabnew ..", &lwin, CIT_COMMAND));
	assert_int_equal(2, tabs_count(&lwin));

	tab_info_t tab_info;
	assert_true(tabs_get(&lwin, 1, &tab_info));

	assert_true(paths_are_same(lwin.curr_dir, test_data));
}

TEST(newtab_fails_in_diff_mode_for_tab_panes)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");

	cfg.pane_tabs = 1;
	(void)compare_two_panes(CT_CONTENTS, LT_ALL, CF_GROUP_PATHS | CF_SHOW);
	assert_failure(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(&lwin));
}

TEST(tab_name_is_set)
{
	tab_info_t tab_info;

	assert_success(cmds_dispatch("tabname new-name", &lwin, CIT_COMMAND));

	assert_true(tabs_get(&lwin, 0, &tab_info));
	assert_string_equal("new-name", tab_info.name);
}

TEST(tab_name_is_reset)
{
	tab_info_t tab_info;

	assert_success(cmds_dispatch("tabname new-name", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("tabname", &lwin, CIT_COMMAND));

	assert_true(tabs_get(&lwin, 0, &tab_info));
	assert_string_equal(NULL, tab_info.name);
}

TEST(tab_is_closed)
{
	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("tabclose", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(&lwin));
}

TEST(last_tab_is_not_closed)
{
	assert_success(cmds_dispatch("tabclose", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(&lwin));
}

TEST(quit_commands_close_tabs)
{
	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("quit", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(&lwin));

	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("wq", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(&lwin));

	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	(void)vle_keys_exec_timed_out(WK_Z WK_Z);
	assert_int_equal(1, tabs_count(&lwin));

	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	(void)vle_keys_exec_timed_out(WK_Z WK_Q);
	assert_int_equal(1, tabs_count(&lwin));
}

TEST(quit_all_commands_ignore_tabs)
{
	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));

	vifm_tests_exited = 0;
	assert_success(cmds_dispatch("qall", &lwin, CIT_COMMAND));
	assert_true(vifm_tests_exited);

	vifm_tests_exited = 0;
	assert_success(cmds_dispatch("wqall", &lwin, CIT_COMMAND));
	assert_true(vifm_tests_exited);

	vifm_tests_exited = 0;
	assert_success(cmds_dispatch("xall", &lwin, CIT_COMMAND));
	assert_true(vifm_tests_exited);

	assert_int_equal(2, tabs_count(&lwin));
}

TEST(tabs_are_switched_with_shortcuts)
{
	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));

	(void)vle_keys_exec_timed_out(WK_g WK_t);
	assert_int_equal(0, tabs_current(&lwin));

	(void)vle_keys_exec_timed_out(WK_g WK_T);
	assert_int_equal(1, tabs_current(&lwin));

	(void)vle_keys_exec_timed_out(L"1" WK_g WK_t);
	assert_int_equal(0, tabs_current(&lwin));
}

TEST(tabs_are_switched_with_commands)
{
	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));

	/* Valid arguments. */

	assert_success(cmds_dispatch("tabnext", &lwin, CIT_COMMAND));
	assert_int_equal(0, tabs_current(&lwin));

	assert_success(cmds_dispatch("tabnext", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(&lwin));

	assert_success(cmds_dispatch("tabnext 1", &lwin, CIT_COMMAND));
	assert_int_equal(0, tabs_current(&lwin));

	assert_success(cmds_dispatch("tabnext 1", &lwin, CIT_COMMAND));
	assert_int_equal(0, tabs_current(&lwin));

	assert_success(cmds_dispatch("tabnext 2", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(&lwin));

	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	assert_int_equal(2, tabs_current(&lwin));

	assert_success(cmds_dispatch("tabprevious", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(&lwin));

	assert_success(cmds_dispatch("tabprevious 2", &lwin, CIT_COMMAND));
	assert_int_equal(2, tabs_current(&lwin));

	assert_success(cmds_dispatch("tabprevious 3", &lwin, CIT_COMMAND));
	assert_int_equal(2, tabs_current(&lwin));

	assert_success(cmds_dispatch("tabprevious 4", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(&lwin));

	/* Invalid arguments. */

	assert_failure(cmds_dispatch("tabnext 1z", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(&lwin));

	assert_failure(cmds_dispatch("tabnext -1", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(&lwin));

	assert_failure(cmds_dispatch("tabnext 4", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(&lwin));

	assert_failure(cmds_dispatch("tabnext 10", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(&lwin));

	assert_failure(cmds_dispatch("tabprevious 0", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(&lwin));

	assert_failure(cmds_dispatch("tabprevious -1", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(&lwin));

	assert_failure(cmds_dispatch("tabprevious -1", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(&lwin));
}

TEST(tabs_are_moved)
{
	for (cfg.pane_tabs = 0; cfg.pane_tabs < 2; ++cfg.pane_tabs)
	{
		assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
		assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));

		assert_int_equal(2, tabs_current(&lwin));

		assert_success(cmds_dispatch("tabmove 0", &lwin, CIT_COMMAND));
		assert_int_equal(0, tabs_current(&lwin));
		assert_success(cmds_dispatch("tabmove 1", &lwin, CIT_COMMAND));
		assert_int_equal(0, tabs_current(&lwin));

		assert_success(cmds_dispatch("tabmove 2", &lwin, CIT_COMMAND));
		assert_int_equal(1, tabs_current(&lwin));
		assert_success(cmds_dispatch("tabmove 2", &lwin, CIT_COMMAND));
		assert_int_equal(1, tabs_current(&lwin));

		assert_success(cmds_dispatch("tabmove 3", &lwin, CIT_COMMAND));
		assert_int_equal(2, tabs_current(&lwin));
		assert_success(cmds_dispatch("tabmove 3", &lwin, CIT_COMMAND));
		assert_int_equal(2, tabs_current(&lwin));

		assert_success(cmds_dispatch("tabmove 1", &lwin, CIT_COMMAND));
		assert_int_equal(1, tabs_current(&lwin));
		assert_success(cmds_dispatch("tabmove", &lwin, CIT_COMMAND));
		assert_int_equal(2, tabs_current(&lwin));

		assert_success(cmds_dispatch("tabmove 0", &lwin, CIT_COMMAND));
		assert_int_equal(0, tabs_current(&lwin));
		assert_success(cmds_dispatch("tabmove $", &lwin, CIT_COMMAND));
		assert_int_equal(2, tabs_current(&lwin));

		assert_success(cmds_dispatch("tabmove 0", &lwin, CIT_COMMAND));
		assert_int_equal(0, tabs_current(&lwin));
		assert_failure(cmds_dispatch("tabmove wrong", &lwin, CIT_COMMAND));
		assert_int_equal(0, tabs_current(&lwin));

		tabs_only(&lwin);
	}
}

TEST(view_mode_is_fine_with_tabs)
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "read",
			cwd);
	populate_dir_list(&lwin, 0);

	(void)vle_keys_exec_timed_out(WK_e);
	(void)vle_keys_exec_timed_out(WK_q);

	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	assert_int_equal(2, tabs_count(&lwin));

	(void)vle_keys_exec_timed_out(WK_e);
	(void)vle_keys_exec_timed_out(WK_q);

	assert_success(cmds_dispatch("tabclose", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(&lwin));

	(void)vle_keys_exec_timed_out(WK_e);
	(void)vle_keys_exec_timed_out(WK_q);
}

TEST(left_view_mode_is_fine_with_tabs)
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "read",
			cwd);
	populate_dir_list(&lwin, 0);

	(void)vle_keys_exec_timed_out(WK_e);
	(void)vle_keys_exec_timed_out(WK_C_i);

	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	assert_int_equal(2, tabs_count(&lwin));

	(void)vle_keys_exec_timed_out(WK_SPACE);
	(void)vle_keys_exec_timed_out(WK_e);
	(void)vle_keys_exec_timed_out(WK_C_i);

	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	assert_int_equal(3, tabs_count(&lwin));

	assert_success(cmds_dispatch("q", &lwin, CIT_COMMAND));
	assert_int_equal(2, tabs_count(&lwin));
	assert_success(cmds_dispatch("q", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(&lwin));

	(void)vle_keys_exec_timed_out(WK_SPACE);
	(void)vle_keys_exec_timed_out(WK_q);
}

TEST(hidden_tabs_are_updated_on_cs_invalidation)
{
	char cs[PATH_MAX + 1];
	make_abs_path(cs, sizeof(cs), TEST_DATA_PATH, "color-schemes", NULL);

	strcpy(lwin.curr_dir, cs);
	assert_success(populate_dir_list(&lwin, 0));
	strcpy(rwin.curr_dir, cs);
	assert_success(populate_dir_list(&rwin, 0));

	curr_stats.cs = &cfg.cs;

	assert_success(cmds_dispatch("highlight {*.vifm} cterm=bold", &lwin,
				CIT_COMMAND));
	assert_non_null(cs_get_file_hi(curr_stats.cs, "some.vifm",
				&lwin.dir_entry[0].hi_num));
	assert_non_null(cs_get_file_hi(curr_stats.cs, "some.vifm",
				&rwin.dir_entry[0].hi_num));
	assert_int_equal(0, lwin.dir_entry[0].hi_num);
	assert_int_equal(0, rwin.dir_entry[0].hi_num);

	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));

	tab_info_t tab_info;

	assert_true(tabs_get(&lwin, 0, &tab_info));
	assert_int_equal(0, tab_info.view->dir_entry[0].hi_num);
	assert_int_equal(0, lwin.dir_entry[0].hi_num);
	assert_true(tabs_get(&rwin, 0, &tab_info));
	assert_int_equal(0, tab_info.view->dir_entry[0].hi_num);
	assert_int_equal(0, rwin.dir_entry[0].hi_num);

	assert_success(cmds_dispatch("highlight clear", &lwin, CIT_COMMAND));

	assert_true(tabs_enum(&lwin, 0, &tab_info));
	assert_int_equal(-1, tab_info.view->dir_entry[0].hi_num);
	assert_int_equal(-1, lwin.dir_entry[0].hi_num);
	assert_true(tabs_enum(&rwin, 0, &tab_info));
	assert_int_equal(-1, tab_info.view->dir_entry[0].hi_num);
	assert_int_equal(-1, rwin.dir_entry[0].hi_num);
}

TEST(setting_tabscope_drops_previous_tabs)
{
	assert_false(cfg.pane_tabs);
	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));

	assert_success(cmds_dispatch("set tabscope=pane", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(&lwin));
	assert_int_equal(1, tabs_count(&rwin));
	assert_true(cfg.pane_tabs);

	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	swap_view_roles();
	assert_success(cmds_dispatch("tabnew", &rwin, CIT_COMMAND));

	assert_success(cmds_dispatch("set tabscope=global", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(&lwin));
	assert_false(cfg.pane_tabs);

	assert_success(cmds_dispatch("set tabscope=pane", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(&lwin));
	assert_int_equal(1, tabs_count(&rwin));
	assert_true(cfg.pane_tabs);
}

TEST(tabonly_leave_only_one_global_tab)
{
	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("tabnew", &lwin, CIT_COMMAND));

	assert_success(cmds_dispatch("tabonly", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(&lwin));
}

TEST(tabonly_leaves_only_one_pane_tab)
{
	cfg.pane_tabs = 1;
	assert_success(cmds_dispatch("tabnew", curr_view, CIT_COMMAND));
	assert_success(cmds_dispatch("tabnew", curr_view, CIT_COMMAND));

	assert_success(cmds_dispatch("tabonly", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(curr_view));
}

TEST(tabonly_keeps_inactive_side_intact)
{
	cfg.pane_tabs = 1;
	assert_success(cmds_dispatch("tabnew", curr_view, CIT_COMMAND));
	assert_success(cmds_dispatch("tabnew", curr_view, CIT_COMMAND));
	swap_view_roles();
	assert_success(cmds_dispatch("tabnew", curr_view, CIT_COMMAND));
	swap_view_roles();

	assert_success(cmds_dispatch("tabonly", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_count(curr_view));
	assert_int_equal(2, tabs_count(other_view));
}

TEST(wingo_global_tabs)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), TEST_DATA_PATH, "", NULL);

	cfg.pane_tabs = 0;

	/*
	 * Tab0:
	 *  - test-data
	 *  - test-data
	 * Tab1:
	 *  - test-data/tree
	 *  - test-data
	 * Tab2:
	 *  - test-data/scripts
	 *  - test-data
	 * Tab3:
	 *  - test-data/scripts
	 *  - test-data/read
	 * Tab4:
	 *  - test-data/scripts
	 *  - test-data/rename
	 */
	assert_success(cmds_dispatch1("tabnew tree", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch1("tabnew ../scripts", &lwin, CIT_COMMAND));
	swap_view_roles();
	assert_success(cmds_dispatch1("tabnew read", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch1("tabnew ../rename", &lwin, CIT_COMMAND));

	/* Hits. */

	assert_success(cmds_dispatch1("wingo tree", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(curr_view));
	assert_true(curr_view == &lwin);

	assert_success(cmds_dispatch1("wingo read", &lwin, CIT_COMMAND));
	assert_int_equal(3, tabs_current(curr_view));
	assert_true(curr_view == &rwin);

	char cmd[32];
	snprintf(cmd, sizeof(cmd), "wingo %u", other_view->id);
	assert_success(cmds_dispatch1(cmd, &lwin, CIT_COMMAND));
	assert_int_equal(3, tabs_current(curr_view));
	assert_true(curr_view == &lwin);

	/* Misses. */

	ui_sb_msg("");
	assert_failure(cmds_dispatch1("wingo test-data", &lwin, CIT_COMMAND));
	assert_string_equal("10 views match 'test-data'", ui_sb_last());
	assert_int_equal(3, tabs_current(curr_view));
	assert_true(curr_view == &lwin);

	ui_sb_msg("");
	assert_failure(cmds_dispatch1("wingo no such match", &lwin, CIT_COMMAND));
	assert_string_equal("No view matches 'no such match'", ui_sb_last());
	assert_int_equal(3, tabs_current(curr_view));
	assert_true(curr_view == &lwin);

	ui_sb_msg("");
	assert_failure(cmds_dispatch1("wingo 59999", &lwin, CIT_COMMAND));
	assert_string_equal("Couldn't find view #59999", ui_sb_last());
	assert_int_equal(3, tabs_current(curr_view));
	assert_true(curr_view == &lwin);
}

TEST(wingo_pane_tabs)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), TEST_DATA_PATH, "", NULL);

	cfg.pane_tabs = 1;

	/*
	 * Left:
	 *  - tab0: test-data
	 *  - tab1: test-data/tree
	 *  - tab2: test-data/scripts
	 * Right:
	 *  - tab0: test-data
	 *  - tab1: test-data/read
	 *  - tab2: test-data/rename
	 */
	assert_success(cmds_dispatch1("tabnew tree", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch1("tabnew ../scripts", &lwin, CIT_COMMAND));
	swap_view_roles();
	assert_success(cmds_dispatch1("tabnew read", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch1("tabnew ../rename", &lwin, CIT_COMMAND));

	/* Hits. */

	assert_success(cmds_dispatch1("wingo tree", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(curr_view));
	assert_true(curr_view == &lwin);

	assert_success(cmds_dispatch1("wingo rename", &lwin, CIT_COMMAND));
	assert_int_equal(2, tabs_current(curr_view));
	assert_true(curr_view == &rwin);

	assert_success(cmds_dispatch1("wingo read", &lwin, CIT_COMMAND));
	assert_int_equal(1, tabs_current(curr_view));
	assert_true(curr_view == &rwin);

	/* Misses. */

	ui_sb_msg("");
	assert_failure(cmds_dispatch1("wingo test-data", &lwin, CIT_COMMAND));
	assert_string_equal("6 views match 'test-data'", ui_sb_last());
	assert_int_equal(1, tabs_current(curr_view));
	assert_true(curr_view == &rwin);

	ui_sb_msg("");
	assert_failure(cmds_dispatch1("wingo no such match", &lwin, CIT_COMMAND));
	assert_string_equal("No view matches 'no such match'", ui_sb_last());
	assert_int_equal(1, tabs_current(curr_view));
	assert_true(curr_view == &rwin);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
