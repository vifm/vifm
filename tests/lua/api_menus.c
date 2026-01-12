#include <stic.h>

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

TEST(menus_loadcustom)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), ".", "", NULL);
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), TEST_DATA_PATH, "rename",
			NULL);

	/* Missing fields. */
	BLUA_ENDS(vlua, ": `title` key is mandatory",
			"print(vifm.menus.loadcustom { })");
	BLUA_ENDS(vlua, ": `items` key is mandatory",
			"print(vifm.menus.loadcustom { title = 't' })");

	/* Empty items. */
	GLUA_EQ(vlua, "false",
			"print(vifm.menus.loadcustom { title = 't', items = { } })");

	/* Items of invalid type. */
	GLUA_EQ(vlua, "false",
			"print(vifm.menus.loadcustom({ title = 'title', items = { {} } }))");

	/* Non-navigatable menu. */
	GLUA_EQ(vlua, "true",
			"print(vifm.menus.loadcustom { title = 't', items = { 'a', 'b' } })");
	assert_true(vle_mode_is(MENU_MODE));
	assert_true(menus_get_view(menu_get_current()) == &lwin);
	assert_true(menu_get_view() == &lwin);
	assert_false(menu_get_current()->extra_data);

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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
