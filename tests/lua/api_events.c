#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ops.h"
#include "../../src/status.h"

#include <test-utils.h>

#include "asserts.h"

static vlua_t *vlua;

SETUP()
{
	vlua = vlua_init();
}

TEARDOWN()
{
	vlua_finish(vlua);
}

TEST(bad_event_name)
{
	BLUA_ENDS(vlua, ": No such event: random",
			"vifm.events.listen {"
			"  event = 'random',"
			"  handler = function() end"
			"}");
}

TEST(app_exit_is_called)
{
	GLUA_EQ(vlua, "",
			"i = 0\n"
			"vifm.events.listen {"
			"  event = 'app.exit',"
			"  handler = function() i = i + 1 end"
			"}");

	vlua_events_app_exit(vlua);
	vlua_process_callbacks(vlua);

	GLUA_EQ(vlua, "1", "print(i)");
}

TEST(app_fsop_is_called)
{
	GLUA_EQ(vlua, "",
			"data = {}\n"
			"vifm.events.listen {"
			"  event = 'app.fsop',"
			"  handler = function(info) data[#data + 1] = info end"
			"}");

	conf_setup();
	cfg.use_system_calls = 1;
	curr_stats.vlua = vlua;
	assert_success(os_chdir(SANDBOX_PATH));
	assert_int_equal(OPS_SUCCEEDED, perform_operation(OP_USR, NULL, NULL,
				"echo", NULL));
	assert_int_equal(OPS_SUCCEEDED, perform_operation(OP_MKFILE, NULL, NULL,
				"from", NULL));
	assert_int_equal(OPS_SUCCEEDED, perform_operation(OP_MOVE, NULL, NULL,
				"from", "to"));
	assert_int_equal(OPS_SUCCEEDED, perform_operation(OP_REMOVESL, NULL, NULL,
				"to", NULL));
	vlua_process_callbacks(vlua);
	curr_stats.vlua = NULL;
	cfg.use_system_calls = 0;
	conf_teardown();

	GLUA_EQ(vlua, "3", "print(#data)");
	GLUA_EQ(vlua, "create\tfrom\tnil\tfalse",
			"print(data[1].op, data[1].path, data[1].target, data[1].isdir)");
	GLUA_EQ(vlua, "move\tfrom\tto\tfalse\tfalse\tfalse",
			"print(data[2].op,"
			"      data[2].path,"
			"      data[2].target,"
			"      data[2].fromtrash,"
			"      data[2].totrash,"
			"      data[2].isdir)");
	GLUA_EQ(vlua, "remove\tto\tnil\tfalse",
			"print(data[3].op, data[3].path, data[3].target, data[3].isdir)");
}

TEST(same_handler_is_called_once)
{
	GLUA_EQ(vlua, "",
			"i = 0\n"
			"handler = function() i = i + 1 end\n"
			"vifm.events.listen { event = 'app.exit', handler = handler }"
			"vifm.events.listen { event = 'app.exit', handler = handler }");

	vlua_events_app_exit(vlua);
	vlua_process_callbacks(vlua);
	assert_string_equal("", ui_sb_last());

	GLUA_EQ(vlua, "1", "print(i)");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
