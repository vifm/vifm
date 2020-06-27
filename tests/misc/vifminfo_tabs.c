#include <stic.h>

#include <stdio.h> /* remove() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/cfg/info.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"

SETUP_ONCE()
{
	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", cwd);

	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);

	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN_ONCE()
{
	columns_teardown();

	curr_view = NULL;
	other_view = NULL;
}

SETUP()
{
	view_setup(&lwin);
	view_setup(&rwin);

	opt_handlers_setup();

	init_view_list(&lwin);
	init_view_list(&rwin);

	cfg.vifm_info = VINFO_TABS;
}

TEARDOWN()
{
	opt_handlers_teardown();

	view_teardown(&lwin);
	view_teardown(&rwin);

	cfg.pane_tabs = 0;
	tabs_only(&lwin);

	assert_success(remove(SANDBOX_PATH "/vifminfo.json"));

	cfg.vifm_info = 0;
}

TEST(names_of_global_tabs_are_restored)
{
	tabs_rename(&lwin, "gtab0");
	assert_success(tabs_new("gtab1", NULL));
	assert_success(tabs_new("gtab2", NULL));

	write_info_file();
	tabs_only(&lwin);
	tabs_rename(&lwin, NULL);
	state_load(0);

	assert_int_equal(3, tabs_count(&lwin));
	tab_info_t tab_info;
	assert_true(tabs_enum(&lwin, 0, &tab_info));
	assert_string_equal("gtab0", tab_info.name);
	assert_true(tabs_enum(&lwin, 1, &tab_info));
	assert_string_equal("gtab1", tab_info.name);
	assert_true(tabs_enum(&lwin, 2, &tab_info));
	assert_string_equal("gtab2", tab_info.name);
}

TEST(names_of_pane_tabs_are_restored)
{
	cfg.pane_tabs = 1;

	tabs_rename(&lwin, "ltab0");
	tabs_rename(&rwin, "rtab0");
	assert_success(tabs_new("ltab1", NULL));
	curr_view = &rwin;
	assert_success(tabs_new("rtab1", NULL));
	curr_view = &lwin;

	write_info_file();
	tabs_only(&lwin);
	tabs_rename(&lwin, NULL);
	tabs_only(&rwin);
	tabs_rename(&rwin, NULL);
	state_load(0);

	assert_int_equal(2, tabs_count(&lwin));
	assert_int_equal(2, tabs_count(&rwin));
	tab_info_t tab_info;
	assert_true(tabs_enum(&lwin, 0, &tab_info));
	assert_string_equal("ltab0", tab_info.name);
	assert_true(tabs_enum(&lwin, 1, &tab_info));
	assert_string_equal("ltab1", tab_info.name);
	assert_true(tabs_enum(&rwin, 0, &tab_info));
	assert_string_equal("rtab0", tab_info.name);
	assert_true(tabs_enum(&rwin, 1, &tab_info));
	assert_string_equal("rtab1", tab_info.name);
}

TEST(active_global_tab_is_restored)
{
	assert_success(tabs_new("gtab1", NULL));
	assert_success(tabs_new("gtab2", NULL));

	tabs_goto(1);
	assert_int_equal(1, tabs_current(&lwin));

	write_info_file();
	tabs_only(&lwin);
	state_load(0);

	assert_int_equal(1, tabs_current(&lwin));
}

TEST(active_pane_tab_is_restored)
{
	cfg.pane_tabs = 1;

	assert_success(tabs_new("ltab1", NULL));
	tabs_goto(2);

	curr_view = &rwin;
	assert_success(tabs_new("rtab1", NULL));
	assert_success(tabs_new("rtab2", NULL));
	tabs_goto(2);
	curr_view = &lwin;

	assert_int_equal(1, tabs_current(&lwin));
	assert_int_equal(2, tabs_current(&rwin));

	write_info_file();
	tabs_only(&lwin);
	tabs_only(&rwin);
	state_load(0);

	assert_int_equal(1, tabs_current(&lwin));
	assert_int_equal(2, tabs_current(&rwin));
}

TEST(layout_of_global_tab_is_restored)
{
	cfg.vifm_info |= VINFO_TUI;
	lwin.sort_g[0] = SK_BY_NAME;
	rwin.sort_g[0] = SK_BY_NAME;

	curr_stats.number_of_windows = 2;
	curr_stats.split = VSPLIT;
	assert_success(tabs_new("gtab1", NULL));
	curr_stats.number_of_windows = 1;
	curr_stats.split = HSPLIT;

	write_info_file();
	tabs_only(&lwin);
	state_load(0);

	tabs_goto(0);
	assert_int_equal(2, curr_stats.number_of_windows);
	assert_int_equal(VSPLIT, curr_stats.split);
	tabs_goto(1);
	assert_int_equal(1, curr_stats.number_of_windows);
	assert_int_equal(HSPLIT, curr_stats.split);
}

TEST(layout_of_pane_tab_is_restored)
{
	cfg.vifm_info |= VINFO_TUI;
	lwin.sort_g[0] = SK_BY_NAME;
	rwin.sort_g[0] = SK_BY_NAME;
	lwin.columns = columns_create();
	rwin.columns = columns_create();

	cfg.pane_tabs = 1;

	curr_stats.preview.on = 0;
	assert_success(tabs_new("ltab1", NULL));
	curr_stats.preview.on = 1;

	write_info_file();
	tabs_only(&lwin);
	tabs_only(&rwin);
	state_load(0);

	assert_true(curr_stats.preview.on);
	tabs_goto(0);
	assert_false(curr_stats.preview.on);

	columns_free(lwin.columns);
	lwin.columns = NULL;
	columns_free(rwin.columns);
	rwin.columns = NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
