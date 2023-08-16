#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/variables.h"
#include "../../src/lua/vlua.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/utils/utils.h"
#include "../../src/cmd_core.h"
#include "../../src/event_loop.h"
#include "../../src/status.h"

#include <test-utils.h>

#include "asserts.h"

static vlua_t *vlua;

SETUP()
{
	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);

	vlua = vlua_init();
	curr_view = &lwin;
	other_view = &rwin;

	engine_cmds_setup(/*real_completion=*/0);
}

TEARDOWN()
{
	vlua_finish(vlua);
	engine_cmds_teardown();
}

TEST(print_outputs_to_statusbar)
{
	GLUA_EQ(vlua, "arg1\targ2", "print('arg1', 'arg2')");
}

TEST(os_getenv_works)
{
	init_variables();

	GLUA_EQ(vlua, "nil", "print(os.getenv('VIFM_TEST'))");

	assert_success(let_variables("$VIFM_TEST='test'"));

	GLUA_EQ(vlua, "test", "print(os.getenv('VIFM_TEST'))");

	clear_variables();
}

TEST(vifm_errordialog)
{
	BLUA_ENDS(vlua,
			": bad argument #2 to 'errordialog' (string expected, got no value)",
			"vifm.errordialog('title')");
	GLUA_EQ(vlua, "", "vifm.errordialog('title', 'msg')");
}

TEST(fnamemodify)
{
	GLUA_EQ(vlua, "c",
			"print(vifm.fnamemodify('/a/b/c.d', ':t:r'))");
	GLUA_EQ(vlua, "/parent/c.d",
			"print(vifm.fnamemodify('c.d', ':p', '/parent'))");
}

TEST(vifm_escape)
{
	GLUA_EQ(vlua, get_env_type() == ET_UNIX ? "\\ " : "\" \"",
			"print(vifm.escape(' '))");
}

TEST(vifm_exists)
{
	GLUA_EQ(vlua, "", "testdata = '" TEST_DATA_PATH "'");

	GLUA_EQ(vlua, "n",
			"print(vifm.exists('/no/such/path!') and 'y' or 'n')");
	GLUA_EQ(vlua, "y",
			"print(vifm.exists(testdata) and 'y' or 'n')");
	GLUA_EQ(vlua, "y",
			"print(vifm.exists(testdata..'/existing-files/a') and 'y' or 'n')");
}

TEST(vifm_executable)
{
	const char *const exec_file = SANDBOX_PATH "/exec" EXE_SUFFIX;
	create_executable(exec_file);

	GLUA_EQ(vlua, "n",
			"print(vifm.executable('.') and 'y' or 'n')");
	GLUA_EQ(vlua, "n",
			"print(vifm.executable('" SANDBOX_PATH "') and 'y' or 'n')");
	GLUA_EQ(vlua, "y",
			"print(vifm.executable('" SANDBOX_PATH "/exec" EXE_SUFFIX "')"
			"      and 'y' or 'n')");
	GLUA_EQ(vlua, "n",
			"print(vifm.executable('" TEST_DATA_PATH "/read/two-lines')"
			"      and 'y' or 'n')");

	assert_success(remove(exec_file));
}

TEST(vifm_makepath)
{
	GLUA_EQ(vlua, "", "sandbox = '" SANDBOX_PATH "'");

	GLUA_EQ(vlua, "", "vifm.makepath(sandbox..'/dir')");
	remove_dir(SANDBOX_PATH "/dir");

	GLUA_EQ(vlua, "", "vifm.makepath(sandbox..'/dir1/dir2')");
	remove_dir(SANDBOX_PATH "/dir1/dir2");
	remove_dir(SANDBOX_PATH "/dir1");
}

TEST(vifm_expand)
{
	copy_str(curr_view->curr_dir, sizeof(curr_view->curr_dir), "/tst");

	GLUA_EQ(vlua, "/tst", "print(vifm.expand('%d'))");
	GLUA_EQ(vlua, "\\\\tst", "print(vifm.expand('%d:p:gs!/!\\\\\\\\!'))");
}

TEST(vifmview_cd)
{
	stub_colmgr();
	conf_setup();
	view_setup(curr_view);

	GLUA_EQ(vlua, "", "testdata = '" TEST_DATA_PATH "'");

	copy_str(curr_view->curr_dir, sizeof(curr_view->curr_dir), "/tst");

	GLUA_EQ(vlua, "y", "print(vifm.currview():cd(testdata) and 'y' or 'n')");

	view_teardown(curr_view);
	conf_teardown();
}

TEST(sb_info_outputs_to_statusbar)
{
	curr_stats.save_msg = 0;
	GLUA_EQ(vlua, "info", "vifm.sb.info 'info'");
	assert_int_equal(1, curr_stats.save_msg);
}

TEST(sb_error_outputs_to_statusbar)
{
	curr_stats.save_msg = 0;
	GLUA_EQ(vlua, "err", "vifm.sb.error 'err'");
	assert_int_equal(1, curr_stats.save_msg);
}

TEST(sb_quick_message_is_not_stored)
{
	curr_stats.save_msg = 0;
	GLUA_EQ(vlua, "", "vifm.sb.quick 'msg'");
	assert_int_equal(0, curr_stats.save_msg);
}

TEST(vifm_currview)
{
	cmds_init();

	conf_setup();
	cfg.pane_tabs = 0;
	view_setup(curr_view);
	view_setup(other_view);
	opt_handlers_setup();
	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);

	GLUA_EQ(vlua, "", "l = vifm.currview()");
	swap_view_roles();
	GLUA_EQ(vlua, "", "r = vifm.currview()");

	/* Both non-nil and aren't equal. */
	GLUA_EQ(vlua, "y", "print(l and 'y' or 'n')");
	GLUA_EQ(vlua, "y", "print(r and 'y' or 'n')");
	GLUA_EQ(vlua, "y", "print(r ~= l and 'y' or 'n')");

	/* Can access visible views. */
	GLUA_EQ(vlua, "", "r:cd('/')");
	GLUA_EQ(vlua, "", "l:cd('/')");

	/* Can't access view that can't be found. */
	++curr_view->id;
	BLUA_ENDS(vlua, "Invalid VifmView object (associated view is dead)",
			"r:cd('bla')");

	assert_success(cmds_dispatch1("tabnew", curr_view, CIT_COMMAND));

	GLUA_EQ(vlua, "", "l:cd('/')");

	assert_success(cmds_dispatch1("tabonly", curr_view, CIT_COMMAND));

	columns_teardown();
	opt_handlers_teardown();
	view_teardown(curr_view);
	view_teardown(other_view);
	conf_teardown();
}

TEST(vifm_version)
{
	GLUA_EQ(vlua, "false", "print(vifm.version.api.has('feature'))");

	GLUA_EQ(vlua, "0\t1\t0",
			"print(vifm.version.api.major,"
			"      vifm.version.api.minor,"
			"      vifm.version.api.patch)");

	GLUA_EQ(vlua, "true", "print(vifm.version.api.atleast(0, 0, 0))");
	GLUA_EQ(vlua, "true", "print(vifm.version.api.atleast(0, 0, 1))");
	GLUA_EQ(vlua, "true", "print(vifm.version.api.atleast(0, 1))");

	GLUA_EQ(vlua, "false", "print(vifm.version.api.atleast(0, 1, 1))");
	GLUA_EQ(vlua, "false", "print(vifm.version.api.atleast(0, 2))");
	GLUA_EQ(vlua, "false", "print(vifm.version.api.atleast(1))");
}

TEST(vifm_run)
{
	conf_setup();

	BLUA_ENDS(vlua, ": Unrecognized value for `pause`: unknown",
			"print(vifm.run({ cmd = 'exit 3',"
			"                  usetermmux = false,"
			"                  pause = 'unknown' }))");

	/* This waits for user input on Windows. */
#ifndef _WIN32
	GLUA_EQ(vlua, "0",
			"print(vifm.run({ cmd = 'exit 0',"
			"                 usetermmux = false,"
			"                 pause = 'onerror' }))");
	GLUA_EQ(vlua, "1",
			"print(vifm.run({ cmd = 'exit 1',"
			"                 usetermmux = false,"
			"                 pause = 'always' }))");
#endif

	GLUA_EQ(vlua, "2",
			"print(vifm.run({ cmd = 'exit 2',"
			"                 usetermmux = false,"
			"                 pause = 'never' }))");

	conf_teardown();
}

TEST(vifm_input)
{
	view_setup(&lwin);
	view_setup(&rwin);

	cfg.timeout_len = 1;

	modes_init();

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(L"def" WK_C_m);
	GLUA_EQ(vlua, "abcdef",
			"print(vifm.input({ prompt = 'write: ',"
			"                   initial = 'abc',"
			"                   complete = 'dir' }))");

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(L"xyz" WK_C_m);
	GLUA_EQ(vlua, "xyz",
			"print(vifm.input({ prompt = 'write: ', complete = 'file' }))");

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(WK_C_c);
	GLUA_EQ(vlua, "nil", "print(vifm.input({ prompt = 'write:' }))");

	BLUA_ENDS(vlua, ": Unrecognized value for `complete`: bla",
				"print(vifm.input({ prompt = 'write: ', complete = 'bla' }))");

	vle_keys_reset();

	view_teardown(&lwin);
	view_teardown(&rwin);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
