#include <stic.h>

#include <stdio.h> /* puts() */

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

#include <test-utils.h>

static vlua_t *vlua;

SETUP()
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);

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

TEST(print_outputs_to_statusbar)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print('arg1', 'arg2')"));
	assert_string_equal("arg1\targ2", ui_sb_last());
}

TEST(vifm_errordialog)
{
	assert_failure(vlua_run_string(vlua, "vifm.errordialog('title')"));
	assert_success(vlua_run_string(vlua, "vifm.errordialog('title', 'msg')"));
}

TEST(fnamemodify)
{
	ui_sb_msg("");

	assert_success(vlua_run_string(vlua,
				"print(vifm.fnamemodify('/a/b/c.d', ':t:r'))"));
	assert_string_equal("c", ui_sb_last());

	assert_success(vlua_run_string(vlua,
				"print(vifm.fnamemodify('c.d', ':p', '/parent'))"));
	assert_string_equal("/parent/c.d", ui_sb_last());
}

TEST(vifm_exists)
{
	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "testdata = '" TEST_DATA_PATH "'"));

	assert_success(vlua_run_string(vlua,
				"print(vifm.exists('/no/such/path!') and 'y' or 'n')"));
	assert_string_equal("n", ui_sb_last());

	assert_success(vlua_run_string(vlua,
				"print(vifm.exists(testdata) and 'y' or 'n')"));
	assert_string_equal("y", ui_sb_last());

	assert_success(vlua_run_string(vlua,
				"print(vifm.exists(testdata..'/existing-files/a') and 'y' or 'n')"));
	assert_string_equal("y", ui_sb_last());
}

TEST(vifm_makepath)
{
	assert_success(vlua_run_string(vlua, "sandbox = '" SANDBOX_PATH "'"));

	assert_success(vlua_run_string(vlua, "vifm.makepath(sandbox..'/dir')"));
	remove_dir(SANDBOX_PATH "/dir");

	assert_success(vlua_run_string(vlua, "vifm.makepath(sandbox..'/dir1/dir2')"));
	remove_dir(SANDBOX_PATH "/dir1/dir2");
	remove_dir(SANDBOX_PATH "/dir1");
}

/* This test comes before other startjob tests to make it pass faster.  When
 * it's first, there are no other jobs which can slow down receiving errors from
 * the process. */
TEST(vifmjob_errors)
{
	conf_setup();
	ui_sb_msg("");

	assert_success(vlua_run_string(vlua, "info = { cmd = 'echo err 1>&2' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "job:wait()\n"
	                                     "while #job:errors() == 0 do end\n"
	                                     "print(job:errors())"));
	assert_true(starts_with_lit(ui_sb_last(), "err"));

	assert_success(vlua_run_string(vlua, "info = { cmd = 'echo out' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:errors())"));
	assert_string_equal("", ui_sb_last());

	conf_teardown();
}

TEST(vifm_startjob)
{
	conf_setup();

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "job = vifm.startjob({ cmd = 'echo' })\n"
	                                     "print(job:exitcode())"));
	assert_string_equal("0", ui_sb_last());

	conf_teardown();
}

TEST(vifmjob_exitcode)
{
	conf_setup();

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "info = {\n"
	                                     "  cmd = 'exit 41',\n"
	                                     "  visible = true,\n"
	                                     "  description = 'exit 41'\n"
	                                     "}\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:exitcode())"));
	assert_string_equal("41", ui_sb_last());

	conf_teardown();
}

TEST(vifmjob_stdout)
{
	conf_setup();

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "info = { cmd = 'echo out' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "if job:stdout() ~= job:stdout() then\n"
	                                     "  print('Result should be the same')\n"
	                                     "else\n"
	                                     "  print(job:stdout():read('a'))\n"
	                                     "end"));
	assert_true(starts_with_lit(ui_sb_last(), "out"));

	conf_teardown();
}

TEST(vifmjob_stderr)
{
	conf_setup();

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "info = { cmd = 'echo err 1>&2' }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:stdout():read('a'))"));
	assert_string_equal("", ui_sb_last());
	assert_success(vlua_run_string(vlua, "info = { cmd = 'echo err 1>&2',"
	                                     "         mergestreams = true }\n"
	                                     "job = vifm.startjob(info)\n"
	                                     "print(job:stdout():read('a'))"));
	assert_true(starts_with_lit(ui_sb_last(), "err"));

	conf_teardown();
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

TEST(vifm_expand)
{
	copy_str(curr_view->curr_dir, sizeof(curr_view->curr_dir), "/tst");

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.expand('%d'))"));
	assert_string_equal("/tst", ui_sb_last());
}

TEST(vifm_change_dir)
{
	stub_colmgr();
	conf_setup();
	view_setup(curr_view);

	assert_success(vlua_run_string(vlua, "testdata = '" TEST_DATA_PATH "'"));

	copy_str(curr_view->curr_dir, sizeof(curr_view->curr_dir), "/tst");

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua,
				"print(vifm.cd(testdata) and 'y' or 'n')"));
	assert_string_equal("y", ui_sb_last());

	view_teardown(curr_view);
	conf_teardown();
}

TEST(sb_info_outputs_to_statusbar)
{
	ui_sb_msg("");
	curr_stats.save_msg = 0;

	assert_success(vlua_run_string(vlua, "vifm.sb.info 'info'"));

	assert_string_equal("info", ui_sb_last());
	assert_int_equal(1, curr_stats.save_msg);
}

TEST(sb_error_outputs_to_statusbar)
{
	ui_sb_msg("");
	curr_stats.save_msg = 0;

	assert_success(vlua_run_string(vlua, "vifm.sb.error 'err'"));

	assert_string_equal("err", ui_sb_last());
	assert_int_equal(1, curr_stats.save_msg);
}

TEST(sb_quick_message_is_not_stored)
{
	ui_sb_msg("");
	curr_stats.save_msg = 0;

	assert_success(vlua_run_string(vlua, "vifm.sb.quick 'msg'"));

	assert_string_equal("", ui_sb_last());
	assert_int_equal(0, curr_stats.save_msg);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
