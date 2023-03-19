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
	assert_failure(vlua_run_string(vlua, "vifm.keys.add {"
	                                     "  modes = { 'cmdline', 'normal' },"
	                                     "  handler = handler,"
	                                     "}"));
	assert_string_ends_with(": `shortcut` key is mandatory", ui_sb_last());

	assert_failure(vlua_run_string(vlua, "vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  handler = handler,"
	                                     "}"));
	assert_string_ends_with(": `modes` key is mandatory", ui_sb_last());

	assert_failure(vlua_run_string(vlua, "vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'cmdline', 'normal' },"
	                                     "}"));
	assert_string_ends_with(": `handler` key is mandatory", ui_sb_last());

	assert_failure(vlua_run_string(vlua, "vifm.keys.add {"
	                                     "  shortcut = '',"
	                                     "  modes = { 'cmdline', 'normal' },"
	                                     "  handler = handler,"
	                                     "}"));
	assert_string_ends_with(": Shortcut can't be empty or longer than 15",
			ui_sb_last());

	assert_failure(vlua_run_string(vlua, "vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'cmdline', 'normal' },"
	                                     "  handler = handler,"
	                                     "  followedby = 'something',"
	                                     "}"));
	assert_string_ends_with(": Unrecognized value for `followedby`: something",
			ui_sb_last());

	assert_failure(vlua_run_string(vlua, "vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  isselector = 10,"
	                                     "}"));
	assert_string_ends_with(": `isselector` value must be a boolean",
			ui_sb_last());
}

TEST(keys_bad_key_handler)
{
	assert_success(vlua_run_string(vlua, "function badhandler()\n"
	                                     "  adsf()\n"
	                                     "end"));

	assert_success(vlua_run_string(vlua, "print(vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'normal' },"
	                                     "  handler = badhandler,"
	                                     "})"));
	assert_string_equal("true", ui_sb_last());

	(void)vle_keys_exec_timed_out(WK_X);
	assert_string_ends_with(": attempt to call a nil value (global 'adsf')",
			ui_sb_last());
}

TEST(keys_bad_selector_handler)
{
	assert_success(vlua_run_string(vlua, "function badhandler()\n"
	                                     "  adsf()\n"
	                                     "end"));

	assert_success(vlua_run_string(vlua, "print(vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'normal' },"
	                                     "  isselector = true,"
	                                     "  handler = badhandler,"
	                                     "})"));
	assert_string_equal("true", ui_sb_last());

	(void)vle_keys_exec_timed_out(L"yX");
	assert_string_ends_with(": attempt to call a nil value (global 'adsf')",
			ui_sb_last());
}

TEST(keys_bad_selector_return)
{
	assert_success(vlua_run_string(vlua, "function badhandler()\n"
	                                     "  return 1\n"
	                                     "end"));

	assert_success(vlua_run_string(vlua, "print(vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'normal' },"
	                                     "  isselector = true,"
	                                     "  handler = badhandler,"
	                                     "})"));
	assert_string_equal("true", ui_sb_last());

	ui_sb_msg("");
	(void)vle_keys_exec_timed_out(L"yX");
	assert_string_equal("", ui_sb_last());
}

TEST(keys_bad_selector_return_table)
{
	assert_success(vlua_run_string(vlua, "function badhandler()\n"
	                                     "  return {}\n"
	                                     "end"));

	assert_success(vlua_run_string(vlua, "print(vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'normal' },"
	                                     "  isselector = true,"
	                                     "  handler = badhandler,"
	                                     "})"));
	assert_string_equal("true", ui_sb_last());

	ui_sb_msg("");
	(void)vle_keys_exec_timed_out(L"yX");
	assert_string_equal("", ui_sb_last());
}

TEST(keys_bad_selector_index)
{
	assert_success(vlua_run_string(vlua, "function badhandler()\n"
	                                     "  return {"
	                                     "    indexes = { 0, 'notint', 1.5 }"
	                                     "  }\n"
	                                     "end"));

	assert_success(vlua_run_string(vlua, "print(vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'normal' },"
	                                     "  isselector = true,"
	                                     "  handler = badhandler,"
	                                     "})"));
	assert_string_equal("true", ui_sb_last());

	ui_sb_msg("");
	(void)vle_keys_exec_timed_out(L"yX");
	assert_string_equal("", ui_sb_last());
}

TEST(keys_selector_duplicated_indexes)
{
	assert_success(vlua_run_string(vlua, "function badhandler()\n"
	                                     "  return { indexes = { 1, 1 } }\n"
	                                     "end"));

	assert_success(vlua_run_string(vlua, "print(vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'normal' },"
	                                     "  isselector = true,"
	                                     "  handler = badhandler,"
	                                     "})"));
	assert_string_equal("true", ui_sb_last());

	(void)vle_keys_exec_timed_out(L"yX");
	assert_int_equal(1, curr_stats.save_msg);
	assert_string_equal("1 file yanked", ui_sb_last());
}

TEST(keys_add)
{
	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "function handler(info)\n"
	                                     "  if info.count == nil then\n"
	                                     "    print 'count is missing'\n"
	                                     "    return\n"
	                                     "  end\n"
	                                     "  if info.register == nil then\n"
	                                     "    print 'register is missing'\n"
	                                     "    return\n"
	                                     "  end\n"
	                                     "  print(info.count .. info.register)\n"
	                                     "end"));
	assert_string_equal("", ui_sb_last());

	/* Create a mapping. */
	assert_success(vle_keys_user_add(L"X", L"x", NORMAL_MODE, 0));

	/* Replace a mapping. */
	assert_success(vlua_run_string(vlua, "print(vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'cmdline', 'normal' },"
	                                     "  description = 'print a message',"
	                                     "  followedby = 'none',"
	                                     "  handler = handler,"
	                                     "})"));
	assert_string_equal("true", ui_sb_last());
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
	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "function handler(info)"
	                                     "  print('in handler')"
	                                     "  return { indexes = { 1, 2 } }"
	                                     "end"));
	assert_string_equal("", ui_sb_last());

	assert_success(vlua_run_string(vlua, "print(vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'cmdline', 'normal' },"
	                                     "  description = 'print a message',"
	                                     "  isselector = true,"
	                                     "  handler = handler,"
	                                     "})"));
	assert_string_equal("true", ui_sb_last());

	(void)vle_keys_exec_timed_out(L"yX");
	assert_int_equal(1, curr_stats.save_msg);
	assert_string_equal("2 files yanked", ui_sb_last());

	const reg_t *reg = regs_find(DEFAULT_REG_NAME);
	assert_non_null(reg);
	assert_int_equal(2, reg->nfiles);
	assert_string_equal("/lwin/file0", reg->files[0]);
	assert_string_equal("/lwin/file1", reg->files[1]);
}

TEST(keys_add_modes)
{
	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "print(vifm.keys.add {"
	                                     "    shortcut = 'X',"
	                                     "    modes = { 'cmdline', 'nav',"
	                                     "              'normal', 'visual',"
	                                     "              'menus', 'dialogs',"
	                                     "              'view' },"
	                                     "    handler = function() end,"
	                                     "})"));
	assert_string_equal("true", ui_sb_last());

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
	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "function handler(info)"
	                                     "  print(#info.indexes,"
	                                     "         info.indexes[1],"
	                                     "         info.indexes[2])"
	                                     "end"));
	assert_string_equal("", ui_sb_last());

	assert_success(vlua_run_string(vlua, "print(vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'normal' },"
	                                     "  followedby = 'selector',"
	                                     "  handler = handler,"
	                                     "})"));
	assert_string_equal("true", ui_sb_last());

	(void)vle_keys_exec_timed_out(L"Xj");
	assert_int_equal(1, curr_stats.save_msg);
	assert_string_equal("2\t1\t2", ui_sb_last());
}

TEST(keys_followed_by_multikey)
{
	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "function handler(info)"
	                                     "  print(info.keyarg)"
	                                     "end"));
	assert_string_equal("", ui_sb_last());

	assert_success(vlua_run_string(vlua, "print(vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'normal' },"
	                                     "  followedby = 'keyarg',"
	                                     "  handler = handler,"
	                                     "})"));
	assert_string_equal("true", ui_sb_last());

	(void)vle_keys_exec_timed_out(L"Xj");
	assert_int_equal(1, curr_stats.save_msg);
	assert_string_equal("j", ui_sb_last());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
