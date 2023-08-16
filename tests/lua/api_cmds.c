#include <stic.h>

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

#include <test-utils.h>

#include "asserts.h"

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
	cmds_init();

	GLUA_EQ(vlua, "",
			"function handler(info)"
			"  if info.args == nil then"
			"    print 'args is missing'"
			"  end"
			"  if info.argv == nil then"
			"    print 'argsv is missing'"
			"  end"
			"  if info.args ~= info.argv[1] then"
			"    print 'args or argv is wrong'"
			"  end"
			"  print 'msg'"
			"end");
	GLUA_EQ(vlua, "",
			"function badhandler()\n"
			"  adsf()\n"
			"end");

	BLUA_ENDS(vlua, "`handler` key is mandatory",
			"vifm.cmds.add { name = 'cmd' }");
	BLUA_ENDS(vlua, "`name` key is mandatory",
			"vifm.cmds.add { handler = handler }");
	BLUA_ENDS(vlua, "`handler` value must be a function",
			"vifm.cmds.add { name = 'cmd', handler = 10 }");
	BLUA_ENDS(vlua, "`minargs` value must be a number",
			"vifm.cmds.add { name = 'cmd', handler = handler, minargs = 'min' }");
	BLUA_ENDS(vlua, "`maxargs` value must be a number",
			"vifm.cmds.add { name = 'cmd', handler = handler, maxargs = 'max' }");

	GLUA_EQ(vlua, "",
			"vifm.cmds.add {"
			"  name = 'cmd',"
			"  description = 'description',"
			"  handler = handler,"
			"  minargs = 1,"
			"}");
	assert_failure(cmds_dispatch1("cmd arg", curr_view, CIT_COMMAND));
	assert_string_equal("msg", ui_sb_last());
	assert_int_equal(1, curr_stats.save_msg);

	GLUA_EQ(vlua, "",
			"vifm.cmds.add { name = 'bcmd', handler = badhandler, minargs = 0 }");
	assert_failure(cmds_dispatch1("bcmd", curr_view, CIT_COMMAND));
	assert_string_ends_with(": attempt to call a nil value (global 'adsf')",
			ui_sb_last());

	GLUA_EQ(vlua, "",
			"vifm.cmds.add {"
			"  name = 'cmdinf',"
			"  handler = handler,"
			"  minargs = 1,"
			"  maxargs = -1"
			"}");
	assert_failure(cmds_dispatch1("cmdinf arg1 arg2", curr_view, CIT_COMMAND));
	assert_string_equal("msg", ui_sb_last());
	assert_int_equal(1, curr_stats.save_msg);
}

TEST(cmds_command)
{
	BLUA_ENDS(vlua, "Action can't be empty",
			"vifm.cmds.command { name = 'name', action = '' }");

	GLUA_EQ(vlua, "",
			"r = vifm.cmds.command({"
			"  name = 'name',"
			"  action = 'action',"
			"  description = 'descr'"
			"})"
			"if not r then print 'fail' end");

	GLUA_EQ(vlua, "fail",
			"r = vifm.cmds.command { name = 'name', action = 'action' }"
			"if not r then print 'fail' end");

	GLUA_EQ(vlua, "",
			"r = vifm.cmds.command {"
			"  name = 'name',"
			"  action = 'action',"
			"  overwrite = true"
			"}"
			"if not r then print 'fail' end");
}

TEST(cmds_names_with_numbers)
{
	GLUA_EQ(vlua, "",
			"r = vifm.cmds.add {"
			"  name = 'my1',"
			"  handler = function() end"
			"}"
			"if not r then print 'fail' end");

	GLUA_EQ(vlua, "",
			"r = vifm.cmds.command {"
			"  name = 'my2',"
			"  action = 'bla'"
			"}"
			"if not r then print 'fail' end");
}

TEST(cmds_delcommand)
{
	GLUA_EQ(vlua, "",
			"r = vifm.cmds.command {"
			"  name = 'name',"
			"  action = 'action'"
			"}"
			"if not r then print 'fail' end");

	GLUA_EQ(vlua, "",
			"r = vifm.cmds.delcommand('name')"
	    "if not r then print 'fail' end");
}

TEST(cmds_completion)
{
	GLUA_EQ(vlua, "", "function handler() end");
	GLUA_EQ(vlua, "", "function bad() end");
	GLUA_EQ(vlua, "", "function failing() asdfadsf() end");
	GLUA_EQ(vlua, "",
			"function completor(info)"
			"  if info.arg == nil then"
			"    print 'arg m isissing'"
			"  end"
			"  if info.args == nil then"
			"    print 'args is missing'"
			"  end"
			"  if info.argv == nil then"
			"    print 'argsv is missing'"
			"  end"
			"  if info.arg ~= info.args then"
			"    print 'arg or args is wrong'"
			"  end"
			"  if info.arg ~= info.argv[1] then"
			"    print 'arg or argv is wrong'"
			"  end"
			"  return {"
			"    offset = 1,"
			"    matches = {"
			"      'aa',"
			"      {match = 'ab',"
			"       description = 'desc'},"
			"      {description = 'only desc'},"
			"      {match = 'bc'}"
			"    }"
			"  }"
			"end");
	GLUA_EQ(vlua, "",
			"function evilcompletor()\n"
			"  return {"
			"    offset = 0,"
			"    matches = { evilcompletor }"
			"  }"
			"end");

	/* :command without completion. */

	GLUA_EQ(vlua, "",
			"vifm.cmds.add {"
			"  name = 'nocompl',"
			"  handler = handler"
			"}");
	assert_int_equal(7, vle_cmds_complete("nocompl a", NULL));
	assert_int_equal(0, vle_compl_get_count());

	/* :command with working completion. */

	GLUA_EQ(vlua, "",
			"vifm.cmds.add {"
			"  name = 'test',"
			"  handler = handler,"
			"  complete = completor"
			"}");
	vle_compl_reset();
	assert_int_equal(6, vle_cmds_complete("test a", NULL));
	check_next_completion("aa");
	check_next_completion("ab");
	check_next_completion("bc");
	check_next_completion("a");

	/* :command with completion that returns nil. */

	GLUA_EQ(vlua, "",
			"vifm.cmds.add {"
			"  name = 'testcmd',"
			"  handler = handler,"
			"  complete = bad"
			"}");
	vle_compl_reset();
	assert_int_equal(8, vle_cmds_complete("testcmd p", NULL));
	assert_int_equal(0, vle_compl_get_count());

	/* :command with completion that errors. */

	GLUA_EQ(vlua, "",
			"vifm.cmds.add {"
			"  name = 'tcmd',"
			"  handler = handler,"
			"  complete = failing"
			"}");
	vle_compl_reset();
	assert_int_equal(5, vle_cmds_complete("tcmd z", NULL));
	assert_int_equal(0, vle_compl_get_count());

	/* :command with completion that has unsupported values. */

	GLUA_EQ(vlua, "",
			"vifm.cmds.add {"
			"  name = 'evilcmpl',"
			"  handler = handler,"
			"  complete = evilcompletor"
			"}");
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
