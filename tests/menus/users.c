#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/lua/vlua.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"
#include "../lua/asserts.h"

SETUP()
{
	modes_init();
	cmds_init();
	conf_setup();
	undo_setup();

	curr_stats.load_stage = -1;
	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN()
{
	vle_keys_reset();
	conf_teardown();
	undo_teardown();

	curr_stats.load_stage = 0;
	curr_view = NULL;
	other_view = NULL;
}

TEST(v_key)
{
	assert_success(cmds_dispatch("!echo path %M", &lwin, CIT_COMMAND));

	curr_stats.vlua = vlua_init();

	update_string(&cfg.vi_command, "#vifmtest#editor");

	GLUA_EQ(curr_stats.vlua, "",
			"function handler(info) ginfo = info; return { success = false } end");
	GLUA_EQ(curr_stats.vlua, "",
			"vifm.addhandler{ name = 'editor', handler = handler }");

	(void)vle_keys_exec(WK_v);

	GLUA_EQ(curr_stats.vlua, "edit-list", "print(ginfo.action)");
	GLUA_EQ(curr_stats.vlua, "1", "print(#ginfo.entries)");
	GLUA_EQ(curr_stats.vlua, "path", "print(ginfo.entries[1])");
	GLUA_EQ(curr_stats.vlua, "1", "print(ginfo.current)");
	GLUA_EQ(curr_stats.vlua, "false", "print(ginfo.isquickfix)");

	vlua_finish(curr_stats.vlua);
	curr_stats.vlua = NULL;

	(void)vle_keys_exec(WK_q);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
