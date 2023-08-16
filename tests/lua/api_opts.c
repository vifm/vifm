#include <stic.h>

#include "../../src/lua/vlua.h"
#include "../../src/ui/ui.h"

#include <test-utils.h>

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
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	opt_handlers_setup();
}

TEARDOWN()
{
	vlua_finish(vlua);

	view_teardown(&lwin);
	view_teardown(&rwin);

	opt_handlers_teardown();
}

TEST(no_such_option)
{
	GLUA_EQ(vlua, "",  "vifm.opts.global.nooption = 1");
	GLUA_EQ(vlua, "nil", "print(vifm.opts.global.nooption)");
}

TEST(bad_option_value)
{
	BLUA_ENDS(vlua,
			"Illegal character: <y>\n" "Failed to set value of option caseoptions",
			"vifm.opts.global.caseoptions = 'yes'");
}

TEST(local_option)
{
	GLUA_EQ(vlua, "", "vifm.opts.global.dotfiles = false");
	GLUA_EQ(vlua, "nil", "print(vifm.opts.global.dotfiles)");
}

TEST(bool_option)
{
	GLUA_EQ(vlua, "", "vifm.opts.global.wrap = true");
	GLUA_EQ(vlua, "true", "print(vifm.opts.global.wrap)");
	GLUA_EQ(vlua, "", "vifm.opts.global.wrap = false");
	GLUA_EQ(vlua, "false", "print(vifm.opts.global.wrap)");
}

TEST(int_option)
{
	GLUA_EQ(vlua, "",  "vifm.opts.global.scrolloff = 123");
	GLUA_EQ(vlua, "123", "print(vifm.opts.global.scrolloff)");
}

TEST(string_option)
{
	GLUA_EQ(vlua, "",  "vifm.opts.global.vicmd = 'vi-cmd'");
	GLUA_EQ(vlua, "vi-cmd", "print(vifm.opts.global.vicmd)");
}

TEST(string_list_option)
{
	GLUA_EQ(vlua, "", "vifm.opts.global.cdpath = 'a,b,c'");
	GLUA_EQ(vlua, "a,b,c",  "print(vifm.opts.global.cdpath)");
}

TEST(enum_option)
{
	GLUA_EQ(vlua, "",  "vifm.opts.global.dirsize = 'nitems'");
	GLUA_EQ(vlua, "nitems", "print(vifm.opts.global.dirsize)");
}

TEST(set_option)
{
	GLUA_EQ(vlua, "", "vifm.opts.global.confirm = 'delete,permdelete'");
	GLUA_EQ(vlua, "delete,permdelete", "print(vifm.opts.global.confirm)");
}

TEST(charset_option)
{
	GLUA_EQ(vlua, "", "vifm.opts.global.caseoptions = 'pG'");
	GLUA_EQ(vlua, "pG", "print(vifm.opts.global.caseoptions)");
}

TEST(view_option)
{
	GLUA_EQ(vlua, "", "v = vifm.currview()");

	GLUA_EQ(vlua, "", "v.viewopts.vicmd = 'vicmd'");
	GLUA_EQ(vlua, "nil", "print(v.viewopts.vicmd)");
	GLUA_EQ(vlua, "", "v.locopts.vicmd = 'vicmd'");
	GLUA_EQ(vlua, "nil", "print(v.locopts.vicmd)");
	GLUA_EQ(vlua, "", "print(vifm.opts.global.vicmd)");

	GLUA_EQ(vlua, "", "v.viewopts.bla = something");
	GLUA_EQ(vlua, "", "v.locopts.bla = something");
	GLUA_EQ(vlua, "nil", "print(v.viewopts.bla)");
	GLUA_EQ(vlua, "nil", "print(v.locopts.bla)");

	GLUA_EQ(vlua, "", "v.viewopts.dotfiles = false");
	GLUA_EQ(vlua, "falsetrue",
				"print(tostring(v.viewopts.dotfiles)..tostring(v.locopts.dotfiles))");

	GLUA_EQ(vlua, "", "v.locopts.dotfiles = false");
	GLUA_EQ(vlua, "falsefalse",
				"print(tostring(v.viewopts.dotfiles)..tostring(v.locopts.dotfiles))");

	swap_view_roles();
	GLUA_EQ(vlua, "", "v.locopts.dotfiles = true");
	GLUA_EQ(vlua, "truetrue",
				"print(tostring(vifm.currview().viewopts.dotfiles).."
				"               tostring(vifm.currview().locopts.dotfiles))");
	GLUA_EQ(vlua, "falsetrue",
				"print(tostring(v.viewopts.dotfiles)..tostring(v.locopts.dotfiles))");

	assert_true(curr_view == &rwin);
	BLUA_ENDS(vlua, "bad argument #3 to '?' (boolean expected, got string)",
			"v.locopts.dotfiles = 'asdf'");
	assert_true(curr_view == &rwin);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
