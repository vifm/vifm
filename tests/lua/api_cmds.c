#include <stic.h>

#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

#include <test-utils.h>

static vlua_t *vlua;

SETUP()
{
	vlua = vlua_init();
	curr_view = &lwin;
	other_view = &rwin;

	engine_cmds_setup();
}

TEARDOWN()
{
	vlua_finish(vlua);
	engine_cmds_teardown();
}

TEST(cmds_add)
{
	init_commands();

	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "function handler()\n"
	                                     "  print 'msg'\n"
	                                     "end"));
	assert_string_equal("", ui_sb_last());

	assert_success(vlua_run_string(vlua, "function badhandler()\n"
	                                     "  adsf()\n"
	                                     "end"));
	assert_string_equal("", ui_sb_last());

	assert_failure(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  name = 'cmd'"
	                                     "}"));
	assert_true(ends_with(ui_sb_last(), "`handler` key is mandatory"));

	assert_failure(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  handler = handler"
	                                     "}"));
	assert_true(ends_with(ui_sb_last(), "`name` key is mandatory"));

	assert_failure(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  name = 'cmd',"
	                                     "  handler = 10"
	                                     "}"));
	assert_true(ends_with(ui_sb_last(), "`handler` value must be a function"));

	assert_failure(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  name = 'cmd',"
	                                     "  handler = handler,"
	                                     "  minargs = 'min'"
	                                     "}"));
	assert_true(ends_with(ui_sb_last(), "`minargs` value must be a number"));

	assert_failure(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  name = 'cmd',"
	                                     "  handler = handler,"
	                                     "  maxargs = 'max'"
	                                     "}"));
	assert_true(ends_with(ui_sb_last(), "`maxargs` value must be a number"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  name = 'cmd',"
	                                     "  description = 'description',"
	                                     "  handler = handler,"
	                                     "  minargs = 1,"
	                                     "}"));
	assert_string_equal("", ui_sb_last());

	ui_sb_msg("");
	assert_failure(exec_command("cmd arg", curr_view, CIT_COMMAND));
	assert_string_equal("msg", ui_sb_last());
	assert_int_equal(1, curr_stats.save_msg);

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  name = 'bcmd',"
	                                     "  handler = badhandler,"
	                                     "  minargs = 0,"
	                                     "}"));
	assert_string_equal("", ui_sb_last());

	ui_sb_msg("");
	assert_failure(exec_command("bcmd", curr_view, CIT_COMMAND));
	assert_true(ends_with(ui_sb_last(), "to call a nil value (global 'adsf')"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  name = 'cmdinf',"
	                                     "  handler = handler,"
	                                     "  minargs = 1,"
	                                     "  maxargs = -1"
	                                     "}"));
	assert_string_equal("", ui_sb_last());

	ui_sb_msg("");
	assert_failure(exec_command("cmdinf arg1 arg2", curr_view, CIT_COMMAND));
	assert_string_equal("msg", ui_sb_last());
	assert_int_equal(1, curr_stats.save_msg);
}

TEST(cmds_command)
{
	ui_sb_msg("");
	assert_failure(vlua_run_string(vlua, "vifm.cmds.command {"
	                                     "  name = 'name',"
	                                     "  action = ''"
	                                     "}"));
	assert_true(ends_with(ui_sb_last(), "Action can't be empty"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "r = vifm.cmds.command({"
	                                     "  name = 'name',"
	                                     "  action = 'action',"
	                                     "  description = 'descr'"
	                                     "})\n"
	                                     "if not r then print 'fail' end"));
	assert_string_equal("", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "r = vifm.cmds.command {"
	                                     "  name = 'name',"
	                                     "  action = 'action'"
	                                     "}\n"
	                                     "if not r then print 'fail' end"));
	assert_string_equal("fail", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "r = vifm.cmds.command {"
	                                     "  name = 'name',"
	                                     "  action = 'action',"
	                                     "  overwrite = true"
	                                     "}\n"
	                                     "if not r then print 'fail' end"));
	assert_string_equal("", ui_sb_last());
}

TEST(cmds_delcommand)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "r = vifm.cmds.command {"
	                                     "  name = 'name',"
	                                     "  action = 'action'"
	                                     "}\n"
	                                     "if not r then print 'fail' end"));
	assert_string_equal("", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "r = vifm.cmds.delcommand('name')\n"
	                                     "if not r then print 'fail' end"));
	assert_string_equal("", ui_sb_last());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
