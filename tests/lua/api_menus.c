#include <stic.h>

#include "../../src/compat/os.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/lua/vlua.h"
#include "../../src/menus/menus.h"
#include "../../src/modes/menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

#include <test-utils.h>

#include "asserts.h"

static vlua_t *vlua;

SETUP_ONCE()
{
	stub_colmgr();

	curr_view = &lwin;
	other_view = &rwin;

	curr_stats.load_stage = -1;
}

TEARDOWN_ONCE()
{
	curr_view = NULL;
	other_view = NULL;

	curr_stats.load_stage = 0;
}

SETUP()
{
	vlua = vlua_init();

	view_setup(&lwin);
	view_setup(&rwin);

	opt_handlers_setup();
	modes_init();
}

TEARDOWN()
{
	vlua_finish(vlua);

	view_teardown(&lwin);
	view_teardown(&rwin);

	vle_keys_reset();
	opt_handlers_teardown();
}

TEST(menus_loadcustom_bad_invocation)
{
	/* Missing fields. */
	BLUA_ENDS(vlua, ": `title` key is mandatory",
			"print(vifm.menus.loadcustom { })");
	BLUA_ENDS(vlua, ": `items` key is mandatory",
			"print(vifm.menus.loadcustom { title = 't' })");

	/* Empty items. */
	GLUA_EQ(vlua, "false",
			"print(vifm.menus.loadcustom { title = 't', items = { } })");

	/* Items/specs fields of invalid type. */
	BLUA_ENDS(vlua, ": `items` value must be a table",
			"print(vifm.menus.loadcustom({ title = 'title', items = 'wrong' }))");
	BLUA_ENDS(vlua, ": `specs` value must be a table",
			"print(vifm.menus.loadcustom({ title = 'title',"
			                              "items = { 'a' },"
			                              "specs = 'wrong',"
			                              "withnavigation = true }))");

	/* Items/specs of invalid type. */
	GLUA_EQ(vlua, "false",
			"print(vifm.menus.loadcustom({ title = 'title', items = { {} } }))");
	GLUA_EQ(vlua, "false",
			"print(vifm.menus.loadcustom({ title = 'title',"
			                              "items = { 'a' },"
			                              "specs = { {} },"
			                              "withnavigation = true }))");

	/* Inconsistent specs. */
	GLUA_EQ(vlua, "false",
			"print(vifm.menus.loadcustom({ title = 'title',"
			                              "items = { 'a' },"
			                              "specs = { 'a', 'b' },"
			                              "withnavigation = true }))");
	GLUA_EQ(vlua, "false",
			"print(vifm.menus.loadcustom({ title = 'title',"
			                              "items = { 'a', 'b' },"
			                              "specs = { 'a' },"
			                              "withnavigation = true }))");
}

TEST(menus_loadcustom_no_navigation)
{
	/* Non-navigatable menu. */
	GLUA_EQ(vlua, "true",
			"print(vifm.menus.loadcustom { title = 't', items = { 'a', 'b' } })");
	assert_true(vle_mode_is(MENU_MODE));
	assert_true(menus_get_view(menu_get_current()) == &lwin);
	assert_true(menu_get_view() == &lwin);
	assert_false(menu_get_current()->extra_data);
}

TEST(menus_loadcustom_stash)
{
	menus_drop_stash();

	GLUA_EQ(vlua, "true",
			"print(vifm.menus.loadcustom {"
			"  title = 'saved',"
			"  items = { 'a', 'b' },"
			"  stash = true,"
			"  pos = 2,"
			"})");
	assert_false(vle_mode_is(MENU_MODE));

	assert_success(menus_unstash(&lwin));
	assert_true(vle_mode_is(MENU_MODE));
	assert_true(menus_get_view(menu_get_current()) == &lwin);
	assert_string_equal("saved", menu_get_current()->title);
	assert_int_equal(2, menu_get_current()->len);
	assert_string_equal("a", menu_get_current()->items[0]);
	assert_string_equal("b", menu_get_current()->items[1]);
	assert_int_equal(1, menu_get_current()->pos);
	assert_false(menu_get_current()->extra_data);

	menus_drop_stash();
}

TEST(menus_loadcustom_navigation)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), ".", "", NULL);
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), TEST_DATA_PATH, "rename",
			NULL);

	/* Navigatable menu. */
	GLUA_EQ(vlua, "true",
			"print(vifm.menus.loadcustom {"
			"  title = 't',"
			"  items = { 'aaa', 'b' },"
			"  withnavigation = true,"
			"  view = vifm.otherview(),"
			"})");
	assert_true(vle_mode_is(MENU_MODE));
	assert_true(menus_get_view(menu_get_current()) == &rwin);
	assert_true(menu_get_view() == &rwin);
	assert_true(menu_get_current()->extra_data);

	/* Correct origin is used for relative paths. */
	(void)vle_keys_exec_timed_out(WK_CR);
	assert_false(vle_mode_is(MENU_MODE));
	assert_int_equal(3, rwin.list_rows);
	assert_string_equal("aaa", rwin.dir_entry[rwin.list_pos].name);
}

TEST(menus_loadcustom_spec_navigation)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "rename",
			NULL);
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), ".", "", NULL);

	/* Menus use "./" as a base if curr_view is used. */
	assert_success(os_chdir(lwin.curr_dir));

	/* Navigatable menu. */
	GLUA_EQ(vlua, "true",
			"print(vifm.menus.loadcustom {"
			"  title = 't',"
			"  items = { 'open aaa', 'open bbb' },"
			"  specs = { 'aaa:2:3: text', 'b' },"
			"  withnavigation = true,"
			"})");
	assert_true(vle_mode_is(MENU_MODE));
	assert_true(menus_get_view(menu_get_current()) == &lwin);
	assert_true(menu_get_view() == &lwin);
	assert_true(menu_get_current()->extra_data);

	/* Correct origin is used for relative paths. */
	(void)vle_keys_exec_timed_out(WK_CR);
	assert_false(vle_mode_is(MENU_MODE));
	assert_int_equal(3, lwin.list_rows);
	assert_string_equal("aaa", lwin.dir_entry[lwin.list_pos].name);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
