#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/lua/vlua.h"
#include "../../src/modes/menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/plugins.h"
#include "../../src/status.h"

#include <test-utils.h>

SETUP()
{
	conf_setup();
	init_modes();
	init_commands();

	curr_view = &lwin;
	other_view = &rwin;
	view_setup(&lwin);

	curr_stats.load_stage = -1;

	curr_stats.vlua = vlua_init();
	curr_stats.plugs = plugs_create(curr_stats.vlua);

	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	create_dir(SANDBOX_PATH "/plugins");
	create_dir(SANDBOX_PATH "/plugins/plug1");
	create_dir(SANDBOX_PATH "/plugins/plug2");
	make_file(SANDBOX_PATH "/plugins/plug1/init.lua", "return {}");
	make_file(SANDBOX_PATH "/plugins/plug2/init.lua", "return");

	plugs_load(curr_stats.plugs, cfg.config_dir);
	assert_success(exec_commands("plugins", &lwin, CIT_COMMAND));
}

TEARDOWN()
{
	remove_file(SANDBOX_PATH "/plugins/plug1/init.lua");
	remove_file(SANDBOX_PATH "/plugins/plug2/init.lua");
	remove_dir(SANDBOX_PATH "/plugins/plug1");
	remove_dir(SANDBOX_PATH "/plugins/plug2");
	remove_dir(SANDBOX_PATH "/plugins");

	vle_cmds_reset();
	vle_keys_reset();
	conf_teardown();

	view_teardown(&lwin);

	curr_stats.load_stage = 0;

	plugs_free(curr_stats.plugs);
	vlua_finish(curr_stats.vlua);
	curr_stats.plugs = NULL;
	curr_stats.vlua = NULL;
}

TEST(plugins_are_listed)
{
	assert_int_equal(2, menu_get_current()->len);
	const char *status0 = "[loaded] ";
	const char *status1 = "[failed] ";
	if(ends_with(menu_get_current()->items[0], "2"))
	{
		status0 = "[failed] ";
		status1 = "[loaded] ";
	}
	assert_true(starts_with(menu_get_current()->items[0], status0));
	assert_true(starts_with(menu_get_current()->items[1], status1));
}

TEST(gf_press)
{
	char *saved_cwd = save_cwd();

	if(ends_with(menu_get_current()->items[0], "2"))
	{
		(void)vle_keys_exec(WK_j);
	}

	/* Unknown press to check that it doesn't mess things up. */
	(void)vle_keys_exec(WK_x);

	(void)vle_keys_exec(WK_g WK_f);

	restore_cwd(saved_cwd);

	char path[PATH_MAX + 1];
	get_current_full_path(&lwin, sizeof(path), path);
	assert_true(paths_are_same(path, SANDBOX_PATH "/plugins/plug1"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
