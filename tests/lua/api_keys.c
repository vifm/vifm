#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/bracket_notation.h"
#include "../../src/status.h"

#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"

static vlua_t *vlua;

SETUP_ONCE()
{
	init_bracket_notation();
}

SETUP()
{
	vlua = vlua_init();
	curr_stats.vlua = vlua;

	curr_view = &lwin;
	other_view = &rwin;

	init_modes();
}

TEARDOWN()
{
	vlua_finish(vlua);
	curr_stats.vlua = NULL;

	vle_keys_reset();
}

TEST(keys_add_errors)
{
	assert_failure(vlua_run_string(vlua, "vifm.keys.add {"
	                                     "  modes = { 'cmdline', 'normal' },"
	                                     "  handler = handler,"
	                                     "}"));
	assert_true(ends_with(ui_sb_last(), ": `shortcut` key is mandatory"));

	assert_failure(vlua_run_string(vlua, "vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  handler = handler,"
	                                     "}"));
	assert_true(ends_with(ui_sb_last(), ": `modes` key is mandatory"));

	assert_failure(vlua_run_string(vlua, "vifm.keys.add {"
	                                     "  shortcut = 'X',"
	                                     "  modes = { 'cmdline', 'normal' },"
	                                     "}"));
	assert_true(ends_with(ui_sb_last(), ": `handler` key is mandatory"));

	assert_failure(vlua_run_string(vlua, "vifm.keys.add {"
	                                     "  shortcut = '',"
	                                     "  modes = { 'cmdline', 'normal' },"
	                                     "  handler = handler,"
	                                     "}"));
	assert_true(ends_with(ui_sb_last(),
				": Shortcut can't be empty or longer than 15"));
}

TEST(keys_bad_handler)
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
	assert_true(ends_with(ui_sb_last(),
				": global 'adsf' is not callable (a nil value)"));
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

TEST(keys_add_modes)
{
	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "print(vifm.keys.add {"
	                                     "    shortcut = 'X',"
	                                     "    modes = { 'cmdline', 'normal', "
	                                     "              'visual', 'menus', "
	                                     "              'dialogs', 'view' },"
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
