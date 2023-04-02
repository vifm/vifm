#include <stic.h>

#include <test-utils.h>

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

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;
	view_setup(&lwin);

	/* The redraw code updates 'columns' and 'lines'. */
	opt_handlers_setup();
	modes_init();
	cmds_init();

	curr_stats.load_stage = -1;

	curr_stats.vlua = vlua_init();
	curr_stats.plugs = plugs_create(curr_stats.vlua);

	make_abs_path(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH, "", NULL);
	create_dir(SANDBOX_PATH "/plugins");
	create_dir(SANDBOX_PATH "/plugins/plug1");
	create_dir(SANDBOX_PATH "/plugins/plug2");
	make_file(SANDBOX_PATH "/plugins/plug1/init.lua", "return {}");
	make_file(SANDBOX_PATH "/plugins/plug2/init.lua", "return");
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
	opt_handlers_teardown();

	view_teardown(&lwin);

	curr_stats.load_stage = 0;

	plugs_free(curr_stats.plugs);
	vlua_finish(curr_stats.vlua);
	curr_stats.plugs = NULL;
	curr_stats.vlua = NULL;
}

TEST(plugins_are_listed)
{
	if(not_windows())
	{
		assert_success(make_symlink("plug1", SANDBOX_PATH "/plugins/plug3"));
	}

	load_plugins(curr_stats.plugs, cfg.config_dir);
	plugs_sort(curr_stats.plugs);
	assert_success(cmds_dispatch("plugins", &lwin, CIT_COMMAND));

	int first_loaded = starts_with(menu_get_current()->items[0], "[ loaded] ");

	assert_int_equal(not_windows() ? 3 : 2, menu_get_current()->len);
	assert_string_starts_with(first_loaded ? "[ loaded] " : "[skipped] ",
			menu_get_current()->items[0]);
	assert_string_starts_with("[ failed] ", menu_get_current()->items[1]);
	if(not_windows())
	{
		assert_string_starts_with(first_loaded ? "[skipped] " : "[ loaded] ",
				menu_get_current()->items[2]);
		remove_file(SANDBOX_PATH "/plugins/plug3");
	}
}

TEST(gf_press)
{
	load_plugins(curr_stats.plugs, cfg.config_dir);
	assert_success(cmds_dispatch("plugins", &lwin, CIT_COMMAND));

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

TEST(e_press_without_errors)
{
	load_plugins(curr_stats.plugs, cfg.config_dir);
	assert_success(cmds_dispatch("plugins", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_e);
	assert_string_starts_with("[", menu_get_current()->items[0]);
}

TEST(e_press_with_errors)
{
	make_file(SANDBOX_PATH "/plugins/plug1/init.lua", "print 'err1\\nerr2'\n"
	                                                  "return {}");
	make_file(SANDBOX_PATH "/plugins/plug2/init.lua", "print 'err1'\n"
	                                                  "print 'err2'\n"
	                                                  "return");

	load_plugins(curr_stats.plugs, cfg.config_dir);
	assert_success(cmds_dispatch("plugins", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_e);
	assert_string_equal("err1", menu_get_current()->items[0]);
	assert_string_equal("err2", menu_get_current()->items[1]);

	/* Unknown press to check that it doesn't mess things up. */
	(void)vle_keys_exec(WK_x);

	(void)vle_keys_exec(WK_h);
	assert_string_starts_with("[", menu_get_current()->items[0]);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
