#include <stic.h>

#include <sys/stat.h> /* chmod() */
#include <unistd.h> /* chdir() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

static line_stats_t *stats;

SETUP_ONCE()
{
	stats = get_line_stats();
	curr_view = &lwin;
	other_view = &rwin;
}

SETUP()
{
	init_modes();
	modcline_enter(CLS_COMMAND, "", NULL);
}

TEARDOWN()
{
	(void)vle_keys_exec_timed_out(WK_C_c);

	vle_keys_reset();
}

TEST(unfinished_scope_is_terminated_at_the_end)
{
	(void)vle_keys_exec_timed_out(L":if 'a' == 'b'" WK_C_m);
	assert_true(cmds_scoped_empty());

	(void)vle_keys_exec_timed_out(L":if 'a' == 'a' | | else" WK_C_m);
	assert_true(cmds_scoped_empty());
}

TEST(finished_scope_is_terminated_at_the_end)
{
	(void)vle_keys_exec_timed_out(L":if 'a' == 'a' | else | endif" WK_C_m);
	assert_true(cmds_scoped_empty());
}

TEST(unfinished_scope_is_terminated_after_external_editing, IF(not_windows))
{
	FILE *fp;
	char path[PATH_MAX + 1];

	char *const saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(10);
	cfg_resize_histories(0);
	cfg_resize_histories(10);

	update_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type(cfg.shell);

	fp = fopen("./script", "w");
	fputs("#!/bin/sh\n", fp);
	fputs("echo \"if 'a' == 'b'\" > $3\n", fp);
	fclose(fp);
	assert_success(chmod("script", 0777));

	curr_stats.exec_env_type = EET_EMULATOR;
	make_abs_path(path, sizeof(path), SANDBOX_PATH, "script", saved_cwd);
	update_string(&cfg.vi_command, path);

	(void)vle_keys_exec_timed_out(WK_C_g);

	update_string(&cfg.vi_command, NULL);
	update_string(&cfg.shell, NULL);
	assert_success(unlink(path));

	restore_cwd(saved_cwd);

	cfg_resize_histories(0);

	assert_true(cmds_scoped_empty());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
