#include <stic.h>

#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"

#include <test-utils.h>

static vlua_t *vlua;

SETUP()
{
	vlua = vlua_init();

	curr_view = &lwin;
	other_view = &rwin;

	opt_handlers_setup();
}

TEARDOWN()
{
	vlua_finish(vlua);

	opt_handlers_teardown();
}

TEST(no_such_option)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "vifm.opts.global.nooption = 1"));
	assert_success(vlua_run_string(vlua, "print(vifm.opts.global.nooption)"));
	assert_string_equal("nil", ui_sb_last());
}

TEST(bad_option_value)
{
	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua, "vifm.opts.global.caseoptions = 'yes'"));
	assert_true(ends_with(ui_sb_last(), "Illegal character: <y>\n"
				"Failed to set value of option caseoptions"));
}

TEST(local_option)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "vifm.opts.global.dotfiles = false"));
	assert_success(vlua_run_string(vlua, "print(vifm.opts.global.dotfiles)"));
	assert_string_equal("nil", ui_sb_last());
}

TEST(bool_option)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "vifm.opts.global.wrap = true"));
	assert_success(vlua_run_string(vlua, "print(vifm.opts.global.wrap)"));
	assert_string_equal("true", ui_sb_last());
	assert_success(vlua_run_string(vlua, "vifm.opts.global.wrap = false"));
	assert_success(vlua_run_string(vlua, "print(vifm.opts.global.wrap)"));
	assert_string_equal("false", ui_sb_last());
}

TEST(int_option)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "vifm.opts.global.scrolloff = 123"));
	assert_success(vlua_run_string(vlua, "print(vifm.opts.global.scrolloff)"));
	assert_string_equal("123", ui_sb_last());
}

TEST(string_option)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "vifm.opts.global.vicmd = 'vi-cmd'"));
	assert_success(vlua_run_string(vlua, "print(vifm.opts.global.vicmd)"));
	assert_string_equal("vi-cmd", ui_sb_last());
}

TEST(string_list_option)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "vifm.opts.global.cdpath = 'a,b,c'"));
	assert_success(vlua_run_string(vlua, "print(vifm.opts.global.cdpath)"));
	assert_string_equal("a,b,c", ui_sb_last());
}

TEST(enum_option)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "vifm.opts.global.dirsize = 'nitems'"));
	assert_success(vlua_run_string(vlua, "print(vifm.opts.global.dirsize)"));
	assert_string_equal("nitems", ui_sb_last());
}

TEST(set_option)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"vifm.opts.global.confirm = 'delete,permdelete'"));
	assert_success(vlua_run_string(vlua, "print(vifm.opts.global.confirm)"));
	assert_string_equal("delete,permdelete", ui_sb_last());
}

TEST(charset_option)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "vifm.opts.global.caseoptions = 'pG'"));
	assert_success(vlua_run_string(vlua, "print(vifm.opts.global.caseoptions)"));
	assert_string_equal("pG", ui_sb_last());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
