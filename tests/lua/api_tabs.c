#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/status.h"

#include "asserts.h"

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
	GLUA_EQ(vlua, "1", "print(vifm.tabs.getcount())");
	tabs_new(NULL, NULL);
	GLUA_EQ(vlua, "2", "print(vifm.tabs.getcount())");
}

TEST(getcount_pane_tabs)
{
	cfg.pane_tabs = 1;

	GLUA_EQ(vlua, "1", "print(vifm.tabs.getcount())");

	tabs_new(NULL, NULL);

	GLUA_EQ(vlua, "2", "print(vifm.tabs.getcount())");
	GLUA_EQ(vlua, "1", "print(vifm.tabs.getcount { other = true })");

	swap_view_roles();

	GLUA_EQ(vlua, "1", "print(vifm.tabs.getcount())");
	GLUA_EQ(vlua, "2", "print(vifm.tabs.getcount { other = true })");
}

TEST(getcurrent_global_tabs)
{
	tabs_new(NULL, NULL);
	tabs_goto(0);

	GLUA_EQ(vlua, "1", "print(vifm.tabs.getcurrent())");
	tabs_next(1);
	GLUA_EQ(vlua, "2", "print(vifm.tabs.getcurrent())");
}

TEST(getcurrent_pane_tabs)
{
	cfg.pane_tabs = 1;

	tabs_new(NULL, NULL);
	tabs_goto(0);

	GLUA_EQ(vlua, "1", "print(vifm.tabs.getcurrent())");

	tabs_next(1);

	GLUA_EQ(vlua, "2", "print(vifm.tabs.getcurrent())");
	GLUA_EQ(vlua, "1", "print(vifm.tabs.getcurrent { other = true })");

	swap_view_roles();

	GLUA_EQ(vlua, "1", "print(vifm.tabs.getcurrent())");
	GLUA_EQ(vlua, "2", "print(vifm.tabs.getcurrent { other = true })");
}

TEST(getname_global_tabs)
{
	tabs_rename(curr_view, "tab1");
	tabs_new(NULL, NULL);
	tabs_rename(curr_view, "tab2");

	GLUA_EQ(vlua, "tab1", "print(vifm.tabs.get({ index = 1 }):getname())");
	GLUA_EQ(vlua, "tab2", "print(vifm.tabs.get({ index = 2 }):getname())");

	GLUA_EQ(vlua, "tab2", "print(vifm.tabs.get({ }):getname())");
	GLUA_EQ(vlua, "tab2", "print(vifm.tabs.get():getname())");

	BLUA_ENDS(vlua, ": No tab with index -1 on active side",
			"print(vifm.tabs.get({ index = 0 }):getname())");
}

TEST(getname_pane_tabs)
{
	cfg.pane_tabs = 1;

	tabs_rename(curr_view, "tab1");
	tabs_new(NULL, NULL);
	tabs_rename(curr_view, "tab2");

	GLUA_EQ(vlua, "tab1", "print(vifm.tabs.get({ index = 1 }):getname())");
	GLUA_EQ(vlua, "tab2", "print(vifm.tabs.get({ index = 2 }):getname())");
	GLUA_EQ(vlua, "tab2", "print(vifm.tabs.get({ }):getname())");

	swap_view_roles();

	GLUA_EQ(vlua, "", "print(vifm.tabs.get({ index = 1 }):getname())");

	GLUA_EQ(vlua, "tab1",
			"print(vifm.tabs.get({ index = 1, other = true }):getname())");
	GLUA_EQ(vlua, "tab2",
			"print(vifm.tabs.get({ other = true }):getname())");
}

TEST(getname_does_not_return_nil)
{
	GLUA_EQ(vlua, "", "print(vifm.tabs.get({ index = 1 }):getname())");
}

TEST(getview_errors)
{
	BLUA_ENDS(vlua, ": pane field is not in the range [1; 2]",
				"print(vifm.tabs.get({ index = 1 }):getview({ pane = 0 }).cwd)");
	BLUA_ENDS(vlua, ": Parameter #2 value must be a table",
				"print(vifm.tabs.get({ index = 1 }):getview(0))");
}

TEST(getview_global_tabs)
{
	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), "t1vl");
	copy_str(rwin.curr_dir, sizeof(rwin.curr_dir), "t1vr");
	tabs_new(NULL, NULL);
	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), "t2vl");
	copy_str(rwin.curr_dir, sizeof(rwin.curr_dir), "t2vr");

	GLUA_EQ(vlua, "t1vl",
			"print(vifm.tabs.get({ index = 1 }):getview().cwd)");
	GLUA_EQ(vlua, "t1vl",
			"print(vifm.tabs.get({ index = 1 }):getview({ pane = 1 }).cwd)");

	/* Changing active pane should have no affect on operation of getview(). */
	swap_view_roles();

	GLUA_EQ(vlua, "t1vr",
			"print(vifm.tabs.get({ index = 1 }):getview({ pane = 2 }).cwd)");
	GLUA_EQ(vlua, "t2vr",
			"print(vifm.tabs.get{}:getview{}.cwd)");

	/* Accessing dead tab should fail. */
	GLUA_EQ(vlua, "",  "tab = vifm.tabs.get({ index = 2 })");
	tabs_close();
	BLUA_ENDS(vlua, ": Invalid VifmTab object (associated tab is dead)",
			"tab:getview()");
}

TEST(getview_pane_tabs)
{
	cfg.pane_tabs = 1;

	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), "t1vl");
	tabs_new(NULL, NULL);
	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), "t2vl");

	GLUA_EQ(vlua, "t1vl",
			"print(vifm.tabs.get({ index = 1 }):getview().cwd)");
	GLUA_EQ(vlua, "t1vl",
			"print(vifm.tabs.get({ index = 1 }):getview({ pane = 1 }).cwd)");
	GLUA_EQ(vlua, "t1vl",
			"print(vifm.tabs.get({ index = 1 }):getview({ pane = 2 }).cwd)");

	/* Accessing dead tab */
	GLUA_EQ(vlua, "", "tab = vifm.tabs.get({ index = 2 })");
	tabs_close();
	BLUA_ENDS(vlua, ": Invalid VifmTab object (associated tab is dead)",
			"tab:getview()");
}

TEST(getlayout_global_tabs)
{
	tabs_rename(curr_view, "tab1");
	tabs_new(NULL, NULL);
	tabs_rename(curr_view, "tab2");

	curr_stats.number_of_windows = 1;

	GLUA_EQ(vlua, "h", "print(vifm.tabs.get({ index = 1 }):getlayout().split)");
	GLUA_EQ(vlua, "true", "print(vifm.tabs.get({ index = 2 }):getlayout().only)");

	curr_stats.number_of_windows = 2;
}

TEST(getlayout_pane_tabs)
{
	cfg.pane_tabs = 1;

	tabs_rename(curr_view, "tab1");
	tabs_new(NULL, NULL);
	tabs_rename(curr_view, "tab2");

	GLUA_EQ(vlua, "false",
			"print(vifm.tabs.get({ index = 1 }):getlayout().only)");

	curr_stats.number_of_windows = 1;

	GLUA_EQ(vlua, "true", "print(vifm.tabs.get({ index = 2 }):getlayout().only)");

	swap_view_roles();

	GLUA_EQ(vlua, "nil", "print(vifm.tabs.get():getlayout().split)");

	curr_stats.number_of_windows = 2;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
