#include <stic.h>

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

#include <test-utils.h>

static void check_next_completion(const char expected[]);

static vlua_t *vlua;

SETUP()
{
	vlua = vlua_init();
	curr_stats.vlua = vlua;

	curr_view = &lwin;
	other_view = &rwin;

	engine_cmds_setup(/*real_completion=*/1);
}

TEARDOWN()
{
	vlua_finish(vlua);
	curr_stats.vlua = NULL;

	engine_cmds_teardown();
}

TEST(cmds_add)
{
	init_commands();

	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "function handler(info)\n"
	                                     "  if info.args == nil then\n"
	                                     "    print 'args is missing'\n"
	                                     "  end\n"
	                                     "  if info.argv == nil then\n"
	                                     "    print 'argsv is missing'\n"
	                                     "  end\n"
	                                     "  if info.args ~= info.argv[1] then\n"
	                                     "    print 'args or argv is wrong'\n"
	                                     "  end\n"
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
	assert_true(ends_with(ui_sb_last(),
				": attempt to call a nil value (global 'adsf')"));

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

TEST(cmds_names_with_numbers)
{
	assert_success(vlua_run_string(vlua, "r = vifm.cmds.add {"
	                                     "  name = 'my1',"
	                                     "  handler = function() end"
	                                     "}\n"
	                                     "if not r then print 'fail' end"));
	assert_string_equal("", ui_sb_last());

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "r = vifm.cmds.command {"
	                                     "  name = 'my2',"
	                                     "  action = 'bla'"
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

TEST(cmds_completion)
{
	assert_success(vlua_run_string(vlua, "function handler()\n"
	                                     "end"));
	assert_string_equal("", ui_sb_last());
	assert_success(vlua_run_string(vlua, "function bad()\n"
	                                     "end"));
	assert_string_equal("", ui_sb_last());
	assert_success(vlua_run_string(vlua, "function failing()\n"
	                                     "  asdfadsf()\n"
	                                     "end"));
	assert_string_equal("", ui_sb_last());
	assert_success(vlua_run_string(vlua, "function completor(info)\n"
	                                     "  if info.arg == nil then\n"
	                                     "    print 'arg m isissing'\n"
	                                     "  end\n"
	                                     "  if info.args == nil then\n"
	                                     "    print 'args is missing'\n"
	                                     "  end\n"
	                                     "  if info.argv == nil then\n"
	                                     "    print 'argsv is missing'\n"
	                                     "  end\n"
	                                     "  if info.arg ~= info.args then\n"
	                                     "    print 'arg or args is wrong'\n"
	                                     "  end\n"
	                                     "  if info.arg ~= info.argv[1] then\n"
	                                     "    print 'arg or argv is wrong'\n"
	                                     "  end\n"
	                                     "  return {\n"
	                                     "    offset = 1,\n"
	                                     "    matches = {\n"
	                                     "      'aa',\n"
	                                     "      {match = 'ab',\n"
	                                     "       description = 'desc'},\n"
	                                     "      {description = 'only desc'},\n"
	                                     "      {match = 'bc'} \n"
	                                     "    }\n"
	                                     "  }\n"
	                                     "end"));
	assert_string_equal("", ui_sb_last());
	assert_success(vlua_run_string(vlua, "function evilcompletor()\n"
	                                     "  return {"
	                                     "    offset = 0,"
	                                     "    matches = { evilcompletor }"
	                                     "  }"
	                                     "end"));
	assert_string_equal("", ui_sb_last());

	/* :command without completion. */

	assert_success(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  name = 'nocompl',"
	                                     "  handler = handler,"
	                                     "}"));
	assert_string_equal("", ui_sb_last());

	assert_int_equal(7, vle_cmds_complete("nocompl a", NULL));
	assert_int_equal(0, vle_compl_get_count());

	/* :command with working completion. */

	assert_success(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  name = 'test',"
	                                     "  handler = handler,"
	                                     "  complete = completor"
	                                     "}"));
	assert_string_equal("", ui_sb_last());

	vle_compl_reset();
	assert_int_equal(6, vle_cmds_complete("test a", NULL));
	check_next_completion("aa");
	check_next_completion("ab");
	check_next_completion("bc");
	check_next_completion("a");

	/* :command with completion that returns nil. */

	assert_success(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  name = 'testcmd',"
	                                     "  handler = handler,"
	                                     "  complete = bad"
	                                     "}"));
	assert_string_equal("", ui_sb_last());
	vle_compl_reset();
	assert_int_equal(8, vle_cmds_complete("testcmd p", NULL));
	assert_int_equal(0, vle_compl_get_count());

	/* :command with completion that errors. */

	assert_success(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  name = 'tcmd',"
	                                     "  handler = handler,"
	                                     "  complete = failing"
	                                     "}"));
	assert_string_equal("", ui_sb_last());
	vle_compl_reset();
	assert_int_equal(5, vle_cmds_complete("tcmd z", NULL));
	assert_int_equal(0, vle_compl_get_count());

	/* :command with completion that has unsupported values. */

	assert_success(vlua_run_string(vlua, "vifm.cmds.add {"
	                                     "  name = 'evilcmpl',"
	                                     "  handler = handler,"
	                                     "  complete = evilcompletor"
	                                     "}"));
	assert_string_equal("", ui_sb_last());
	vle_compl_reset();
	assert_int_equal(9, vle_cmds_complete("evilcmpl z", NULL));
	assert_int_equal(1, vle_compl_get_count());
}

static void
check_next_completion(const char expected[])
{
	char *buf = vle_compl_next();
	assert_string_equal(expected, buf);
	free(buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
