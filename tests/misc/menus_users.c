#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/lua/vlua.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

SETUP()
{
	init_modes();
	init_commands();
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
	assert_success(exec_commands("!echo path %M", &lwin, CIT_COMMAND));

	curr_stats.vlua = vlua_init();

	update_string(&cfg.vi_command, "#vifmtest#editor");

	assert_success(vlua_run_string(curr_stats.vlua,
				"function handler(info) ginfo = info; return { success = false } end"));
	assert_success(vlua_run_string(curr_stats.vlua,
				"vifm.addhandler{ name = 'editor', handler = handler }"));

	(void)vle_keys_exec(WK_v);

	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.action)"));
	assert_string_equal("edit-list", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(#ginfo.entries)"));
	assert_string_equal("1", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.entries[1])"));
	assert_string_equal("path", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.current)"));
	assert_string_equal("1", ui_sb_last());
	assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.isquickfix)"));
	assert_string_equal("false", ui_sb_last());

	vlua_finish(curr_stats.vlua);
	curr_stats.vlua = NULL;

	(void)vle_keys_exec(WK_q);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
