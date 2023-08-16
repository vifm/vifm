#include <stic.h>

#include <string.h>

#include "../../src/engine/keys.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/bracket_notation.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/registers.h"
#include "../../src/status.h"

#include <test-utils.h>

#include "asserts.h"

static vlua_t *vlua;

SETUP_ONCE()
{
	init_bracket_notation();
	stub_colmgr();
}

SETUP()
{
	vlua = vlua_init();
	curr_stats.vlua = vlua;

	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);
	strcpy(lwin.curr_dir, "/lwin");
	lwin.list_rows = 2;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("file0");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("file1");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];

	modes_init();
	regs_init();
}

TEARDOWN()
{
	view_teardown(&lwin);

	vlua_finish(vlua);
	curr_stats.vlua = NULL;

	vle_keys_reset();
	regs_reset();
}

TEST(keys_add_errors)
{
	BLUA_ENDS(vlua, ": `shortcut` key is mandatory",
			"vifm.keys.add {"
			"  modes = { 'cmdline', 'normal' },"
			"  handler = handler,"
			"}");

	BLUA_ENDS(vlua, ": `modes` key is mandatory",
			"vifm.keys.add {"
			"  shortcut = 'X',"
			"  handler = handler,"
			"}");

	BLUA_ENDS(vlua, ": `handler` key is mandatory",
			"vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'cmdline', 'normal' },"
			"}");

	BLUA_ENDS(vlua, ": Shortcut can't be empty or longer than 15",
			"vifm.keys.add {"
			"  shortcut = '',"
			"  modes = { 'cmdline', 'normal' },"
			"  handler = handler,"
			"}");

	BLUA_ENDS(vlua, ": Unrecognized value for `followedby`: something",
			"vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'cmdline', 'normal' },"
			"  handler = handler,"
			"  followedby = 'something',"
			"}");

	BLUA_ENDS(vlua, ": `isselector` value must be a boolean",
			"vifm.keys.add {"
			"  shortcut = 'X',"
			"  isselector = 10,"
			"}");
}

TEST(keys_bad_key_handler)
{
	GLUA_EQ(vlua, "true",
			"print(vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'normal' },"
			"  handler = function() adsf() end,"
			"})");

	ui_sb_msg("");
	(void)vle_keys_exec_timed_out(WK_X);
	assert_string_ends_with(": attempt to call a nil value (global 'adsf')",
			ui_sb_last());
}

TEST(keys_bad_selector_handler)
{
	GLUA_EQ(vlua, "true",
			"print(vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'normal' },"
			"  isselector = true,"
			"  handler = function() adsf() end,"
			"})");

	ui_sb_msg("");
	(void)vle_keys_exec_timed_out(L"yX");
	assert_string_ends_with(": attempt to call a nil value (global 'adsf')",
			ui_sb_last());
}

TEST(keys_bad_selector_return)
{
	GLUA_EQ(vlua, "true",
			"print(vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'normal' },"
			"  isselector = true,"
			"  handler = function() return 1 end,"
			"})");

	ui_sb_msg("");
	(void)vle_keys_exec_timed_out(L"yX");
	assert_string_equal("", ui_sb_last());
}

TEST(keys_bad_selector_return_table)
{
	GLUA_EQ(vlua, "true",
			"print(vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'normal' },"
			"  isselector = true,"
			"  handler = function() return {} end,"
			"})");

	ui_sb_msg("");
	(void)vle_keys_exec_timed_out(L"yX");
	assert_string_equal("", ui_sb_last());
}

TEST(keys_bad_selector_index)
{
	GLUA_EQ(vlua, "true",
			"print(vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'normal' },"
			"  isselector = true,"
			"  handler = function() return { indexes = { 0, 'notint', 1.5 } } end,"
			"})");

	ui_sb_msg("");
	(void)vle_keys_exec_timed_out(L"yX");
	assert_string_equal("", ui_sb_last());
}

TEST(keys_selector_duplicated_indexes)
{
	GLUA_EQ(vlua, "true",
			"print(vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'normal' },"
			"  isselector = true,"
			"  handler = function() return { indexes = { 1, 1 } } end,"
			"})");

	ui_sb_msg("");
	(void)vle_keys_exec_timed_out(L"yX");
	assert_int_equal(1, curr_stats.save_msg);
	assert_string_equal("1 file yanked", ui_sb_last());
}

TEST(keys_bad_selector_cursorpos)
{
	GLUA_EQ(vlua, "",
			"function badhandler()"
			"  return {"
			"    indexes = { 1, 2 },"
			"    cursorpos = 1.5,"
			"  }"
			"end");

	GLUA_EQ(vlua, "true",
			"print(vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'normal' },"
			"  isselector = true,"
			"  handler = badhandler,"
			"})");

	(void)vle_keys_exec_timed_out(L"yX");
	assert_int_equal(0, lwin.list_pos);
}

TEST(keys_add)
{
	GLUA_EQ(vlua, "",
			"function handler(info)"
			"  if info.count == nil then"
			"    print 'count is missing'"
			"    return"
			"  end"
			"  if info.register == nil then"
			"    print 'register is missing'"
			"    return"
			"  end"
			"  print(info.count .. info.register)"
			"end");

	/* Create a mapping. */
	assert_success(vle_keys_user_add(L"X", L"x", NORMAL_MODE, 0));

	/* Replace a mapping. */
	GLUA_EQ(vlua, "true",
			"print(vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'cmdline', 'normal' },"
			"  description = 'print a message',"
			"  followedby = 'none',"
			"  handler = handler,"
			"})");
	assert_true(vle_keys_user_exists(L"X", NORMAL_MODE));
	assert_true(vle_keys_user_exists(L"X", CMDLINE_MODE));

	(void)vle_keys_exec_timed_out(L"X");
	assert_int_equal(1, curr_stats.save_msg);
	assert_string_equal("count is missing", ui_sb_last());

	(void)vle_keys_exec_timed_out(L"1X");
	assert_int_equal(1, curr_stats.save_msg);
	assert_string_equal("register is missing", ui_sb_last());

	(void)vle_keys_exec_timed_out(L"\"\"22X");
	assert_int_equal(1, curr_stats.save_msg);
	assert_string_equal("22\"", ui_sb_last());
}

TEST(keys_add_selector)
{
	GLUA_EQ(vlua, "",
			"function handler(info)"
			"  print('in handler')"
			"  return {"
			"    indexes = { 1, 2 },"
			"    cursorpos = 2,"
			"  }"
			"end");

	GLUA_EQ(vlua, "true",
			"print(vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'cmdline', 'normal' },"
			"  description = 'print a message',"
			"  isselector = true,"
			"  handler = handler,"
			"})");

	(void)vle_keys_exec_timed_out(L"yX");
	assert_int_equal(1, curr_stats.save_msg);
	assert_string_equal("2 files yanked", ui_sb_last());

	const reg_t *reg = regs_find(DEFAULT_REG_NAME);
	assert_non_null(reg);
	assert_int_equal(2, reg->nfiles);
	assert_string_equal("/lwin/file0", reg->files[0]);
	assert_string_equal("/lwin/file1", reg->files[1]);

	assert_int_equal(1, lwin.list_pos);
}

TEST(keys_add_modes)
{
	GLUA_EQ(vlua, "true",
			"print(vifm.keys.add {"
			"    shortcut = 'X',"
			"    modes = { 'cmdline', 'nav',"
			"              'normal', 'visual',"
			"              'menus', 'dialogs',"
			"              'view' },"
			"    handler = function() end,"
			"})");

	assert_true(vle_keys_user_exists(L"X", NORMAL_MODE));
	assert_true(vle_keys_user_exists(L"X", CMDLINE_MODE));
	assert_true(vle_keys_user_exists(L"X", VISUAL_MODE));
	assert_true(vle_keys_user_exists(L"X", MENU_MODE));
	assert_true(vle_keys_user_exists(L"X", SORT_MODE));
	assert_true(vle_keys_user_exists(L"X", ATTR_MODE));
	assert_true(vle_keys_user_exists(L"X", CHANGE_MODE));
	assert_true(vle_keys_user_exists(L"X", VIEW_MODE));
	assert_true(vle_keys_user_exists(L"X", FILE_INFO_MODE));
	assert_false(vle_keys_user_exists(L"X", MSG_MODE));
	assert_false(vle_keys_user_exists(L"X", MORE_MODE));
}

TEST(keys_followed_by_selector)
{
	GLUA_EQ(vlua, "true",
			"print(vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'normal' },"
			"  followedby = 'selector',"
			"  handler = function(info)"
			"    print(#info.indexes, info.indexes[1], info.indexes[2])"
			"  end"
			"})");

	(void)vle_keys_exec_timed_out(L"Xj");
	assert_int_equal(1, curr_stats.save_msg);
	assert_string_equal("2\t1\t2", ui_sb_last());
}

TEST(keys_followed_by_multikey)
{
	GLUA_EQ(vlua, "true",
			"print(vifm.keys.add {"
			"  shortcut = 'X',"
			"  modes = { 'normal' },"
			"  followedby = 'keyarg',"
			"  handler = function(info)"
			"    print(info.keyarg)"
			"  end"
			"})");

	(void)vle_keys_exec_timed_out(L"Xj");
	assert_int_equal(1, curr_stats.save_msg);
	assert_string_equal("j", ui_sb_last());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
