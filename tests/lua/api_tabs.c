#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/status.h"

static vlua_t *vlua;

SETUP_ONCE()
{
	stub_colmgr();
}

SETUP()
{
	vlua = vlua_init();

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
	vlua_finish(vlua);

	tabs_only(&lwin);
	tabs_only(&rwin);
	cfg.pane_tabs = 0;
	tabs_only(&lwin);
	tabs_rename(&lwin, "");

	opt_handlers_teardown();

	view_teardown(&lwin);
	view_teardown(&rwin);

	columns_teardown();
}

TEST(getcount_global_tabs)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcount())"));
	assert_string_equal("1", ui_sb_last());

	tabs_new(NULL, NULL);

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcount())"));
	assert_string_equal("2", ui_sb_last());
}

TEST(getcount_pane_tabs)
{
	cfg.pane_tabs = 1;

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcount())"));
	assert_string_equal("1", ui_sb_last());

	tabs_new(NULL, NULL);

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcount())"));
	assert_string_equal("2", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcount {"
				"    other = true"
				"})"));
	assert_string_equal("1", ui_sb_last());

	swap_view_roles();

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcount())"));
	assert_string_equal("1", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcount {"
				"    other = true"
				"})"));
	assert_string_equal("2", ui_sb_last());
}

TEST(getcurrent_global_tabs)
{
	tabs_new(NULL, NULL);
	tabs_goto(0);

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcurrent())"));
	assert_string_equal("1", ui_sb_last());

	tabs_next(1);

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcurrent())"));
	assert_string_equal("2", ui_sb_last());
}

TEST(getcurrent_pane_tabs)
{
	cfg.pane_tabs = 1;

	tabs_new(NULL, NULL);
	tabs_goto(0);

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcurrent())"));
	assert_string_equal("1", ui_sb_last());

	tabs_next(1);

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcurrent())"));
	assert_string_equal("2", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcurrent {"
				"    other = true"
				"})"));
	assert_string_equal("1", ui_sb_last());

	swap_view_roles();

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcurrent())"));
	assert_string_equal("1", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.tabs.getcurrent {"
				"    other = true"
				"})"));
	assert_string_equal("2", ui_sb_last());
}

TEST(getname_global_tabs)
{
	tabs_rename(curr_view, "tab1");
	tabs_new(NULL, NULL);
	tabs_rename(curr_view, "tab2");

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getname())"));
	assert_string_equal("tab1", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 2 }):getname())"));
	assert_string_equal("tab2", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ }):getname())"));
	assert_string_equal("tab2", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get():getname())"));
	assert_string_equal("tab2", ui_sb_last());

	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 0 }):getname())"));
	assert_true(ends_with(ui_sb_last(), ": No tab with index -1 on active side"));
}

TEST(getname_pane_tabs)
{
	cfg.pane_tabs = 1;

	tabs_rename(curr_view, "tab1");
	tabs_new(NULL, NULL);
	tabs_rename(curr_view, "tab2");

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getname())"));
	assert_string_equal("tab1", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 2 }):getname())"));
	assert_string_equal("tab2", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ }):getname())"));
	assert_string_equal("tab2", ui_sb_last());

	swap_view_roles();

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getname())"));
	assert_string_equal("", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1, other = true }):getname())"));
	assert_string_equal("tab1", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ other = true }):getname())"));
	assert_string_equal("tab2", ui_sb_last());
}

TEST(getname_does_not_return_nil)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getname())"));
	assert_string_equal("", ui_sb_last());
}

TEST(getview_errors)
{
	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getview({ pane = 0 }).cwd)"));
	assert_true(ends_with(ui_sb_last(),
				": pane field is not in the range [1; 2]"));

	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getview(0))"));
	assert_true(ends_with(ui_sb_last(),
				": Parameter #2 value must be a table"));
}

TEST(getview_global_tabs)
{
	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), "t1vl");
	copy_str(rwin.curr_dir, sizeof(rwin.curr_dir), "t1vr");
	tabs_new(NULL, NULL);
	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), "t2vl");
	copy_str(rwin.curr_dir, sizeof(rwin.curr_dir), "t2vr");

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getview().cwd)"));
	assert_string_equal("t1vl", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getview({ pane = 1 }).cwd)"));
	assert_string_equal("t1vl", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getview({ pane = 2 }).cwd)"));
	assert_string_equal("t1vr", ui_sb_last());

	/* Accessing dead tab */
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "tab = vifm.tabs.get({ index = 2 })"));
	tabs_close();
	assert_failure(vlua_run_string(vlua, "tab:getview()"));
	assert_true(ends_with(ui_sb_last(),
				": Invalid VifmTab object (associated tab is dead)"));
}

TEST(getview_pane_tabs)
{
	cfg.pane_tabs = 1;

	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), "t1vl");
	tabs_new(NULL, NULL);
	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), "t2vl");

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getview().cwd)"));
	assert_string_equal("t1vl", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getview({ pane = 1 }).cwd)"));
	assert_string_equal("t1vl", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getview({ pane = 2 }).cwd)"));
	assert_string_equal("t1vl", ui_sb_last());

	/* Accessing dead tab */
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "tab = vifm.tabs.get({ index = 2 })"));
	tabs_close();
	assert_failure(vlua_run_string(vlua, "tab:getview()"));
	assert_true(ends_with(ui_sb_last(),
				": Invalid VifmTab object (associated tab is dead)"));
}

TEST(getlayout_global_tabs)
{
	tabs_rename(curr_view, "tab1");
	tabs_new(NULL, NULL);
	tabs_rename(curr_view, "tab2");

	curr_stats.number_of_windows = 1;

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getlayout().split)"));
	assert_string_equal("h", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 2 }):getlayout().only)"));
	assert_string_equal("true", ui_sb_last());

	curr_stats.number_of_windows = 2;
}

TEST(getlayout_pane_tabs)
{
	cfg.pane_tabs = 1;

	tabs_rename(curr_view, "tab1");
	tabs_new(NULL, NULL);
	tabs_rename(curr_view, "tab2");

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 1 }):getlayout().only)"));
	assert_string_equal("false", ui_sb_last());

	curr_stats.number_of_windows = 1;

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get({ index = 2 }):getlayout().only)"));
	assert_string_equal("true", ui_sb_last());

	swap_view_roles();

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.tabs.get():getlayout().split)"));
	assert_string_equal("nil", ui_sb_last());

	curr_stats.number_of_windows = 2;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
