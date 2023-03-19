#include <stic.h>

#include <string.h> /* strdup() */

#include "../../src/engine/keys.h"
#include "../../src/int/file_magic.h"
#include "../../src/lua/vlua.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/visual.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"

#include <test-utils.h>

static int has_mime_type_detection(void);
static int has_no_mime_type_detection(void);

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

TEST(vifmview_custom)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.currview().custom)"));
	assert_string_equal("nil", ui_sb_last());

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), "", "", NULL);
	flist_custom_start(&lwin, "vifmview_custom");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_success(flist_custom_finish(&lwin, CV_REGULAR, /*allow_empty=*/0));

	assert_success(vlua_run_string(vlua,
				"print(vifm.currview().custom.title)"));
	assert_string_equal("vifmview_custom", ui_sb_last());
	assert_success(vlua_run_string(vlua,
				"print(vifm.currview().custom.type)"));
	assert_string_equal("custom", ui_sb_last());
}

TEST(vifmview_select)
{
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);

	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "print(vifm.currview():select({"
	                                     "  indexes = { 0, 1.5, 10 }"
	                                     "}))"));

	assert_string_equal("0", ui_sb_last());
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);

	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "print(vifm.currview():select({"
	                                     "  indexes = { 2 }"
	                                     "}))"));

	assert_string_equal("1", ui_sb_last());
	assert_false(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);

	assert_success(vlua_run_string(vlua, "print(vifm.currview():select({"
	                                     "  indexes = { 1, 2 }"
	                                     "}))"));

	assert_string_equal("1", ui_sb_last());
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
}

TEST(vifmview_select_in_visual_mode)
{
	modes_init();
	opt_handlers_setup();

	/* Does nothing in visual non-amend mode. */

	modvis_enter(VS_NORMAL);

	assert_false(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);

	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "print(vifm.currview():select({"
	                                     "  indexes = { 1 }"
	                                     "}))"));

	assert_string_equal("0", ui_sb_last());
	assert_false(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);

	/* Selects in visual amend mode. */

	modvis_enter(VS_AMEND);

	assert_false(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);

	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "print(vifm.currview():select({"
	                                     "  indexes = { 1 }"
	                                     "}))"));

	assert_string_equal("1", ui_sb_last());
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);

	modvis_leave(/*save_msg=*/0, /*goto_top=*/1, /*clear_selection=*/1);

	(void)vle_keys_exec_timed_out(WK_C_c);
	vle_keys_reset();
	opt_handlers_teardown();
}

TEST(vifmview_unselect)
{
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;

	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "print(vifm.currview():unselect({"
	                                     "  indexes = { 0, 1.5, 10 }"
	                                     "}))"));

	assert_string_equal("0", ui_sb_last());
	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);

	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "print(vifm.currview():unselect({"
	                                     "  indexes = { 2 }"
	                                     "}))"));

	assert_string_equal("1", ui_sb_last());
	assert_true(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);

	assert_success(vlua_run_string(vlua, "print(vifm.currview():unselect({"
	                                     "  indexes = { 1, 2 }"
	                                     "}))"));

	assert_string_equal("1", ui_sb_last());
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
}

TEST(vifmview_entry_mimetype_unavailable, IF(has_no_mime_type_detection))
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.currview():entry(2):mimetype())"));
	assert_string_equal("nil", ui_sb_last());
}

TEST(vifmview_entry_mimetype, IF(has_mime_type_detection))
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", NULL);
	lwin.dir_entry[1].origin = SANDBOX_PATH;
	copy_file(TEST_DATA_PATH "/read/very-long-line", SANDBOX_PATH "/file1");

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.currview():entry(2):mimetype())"));
	assert_string_equal("text/plain", ui_sb_last());

	remove_file(SANDBOX_PATH "/file1");
}

static int
has_mime_type_detection(void)
{
	return get_mimetype(TEST_DATA_PATH "/read/dos-line-endings", 0) != NULL;
}

static int
has_no_mime_type_detection(void)
{
	return has_mime_type_detection() == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
