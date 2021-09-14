#include <stic.h>

#include <string.h> /* strdup() */

#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"

#include <test-utils.h>

static vlua_t *vlua;

SETUP_ONCE()
{
	stub_colmgr();

	curr_view = &lwin;
	other_view = &lwin;
}

TEARDOWN_ONCE()
{
	curr_view = NULL;
	other_view = NULL;
}

SETUP()
{
	vlua = vlua_init();

	view_setup(&lwin);
	strcpy(lwin.curr_dir, "/lwin");
	lwin.list_rows = 2;
	lwin.list_pos = 1;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file0");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("file1");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
}

TEARDOWN()
{
	view_teardown(&lwin);

	vlua_finish(vlua);
}

TEST(vifmview_properties)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.currview().entrycount)"));
	assert_string_equal("2", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.currview().currententry)"));
	assert_string_equal("2", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.currview().cwd)"));
	assert_string_equal("/lwin", ui_sb_last());
}

TEST(vifmview_entry)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.currview():entry(0))"));
	assert_string_equal("nil", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.currview():entry(1).name)"));
	assert_string_equal("file0", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.otherview():entry(2).name)"));
	assert_string_equal("file1", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.currview():entry(3))"));
	assert_string_equal("nil", ui_sb_last());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
