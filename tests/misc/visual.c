#include <stic.h>

#include <unistd.h> /* chdir() symlink() */

#include <limits.h> /* INT_MIN */
#include <string.h> /* memset() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"

#include "utils.h"

static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	curr_view = &lwin;
	other_view = &rwin;
}

SETUP()
{
	view_setup(&lwin);
	init_modes();
	opt_handlers_setup();
}

TEARDOWN()
{
	(void)vle_keys_exec_timed_out(WK_C_c);
	view_teardown(&lwin);
	vle_keys_reset();
	opt_handlers_teardown();
}

TEST(v_after_av_drops_selection)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"existing-files", cwd);
	populate_dir_list(&lwin, /*reload=*/0);

	assert_int_equal(3, lwin.list_rows);

	/* Select first file. */
	(void)vle_keys_exec_timed_out(WK_t);
	/* Select second file. */
	(void)vle_keys_exec_timed_out(WK_j WK_t);

	/* Enter visual mode with amend at the last file. */
	(void)vle_keys_exec_timed_out(WK_j WK_a WK_v);
	/* Switch to regular visual mode. */
	(void)vle_keys_exec_timed_out(WK_v);

	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);

	/* Go up and then back down. */
	(void)vle_keys_exec_timed_out(WK_g WK_g);
	(void)vle_keys_exec_timed_out(WK_G);

	/* Temporarily including previously selected entries shouldn't fixate their
	 * selection. */
	assert_false(lwin.dir_entry[0].selected);
	assert_false(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);
}

TEST(cl, IF(not_windows))
{
	conf_setup();
	undo_setup();

	chdir(SANDBOX_PATH);

	assert_success(make_symlink("target", "symlink"));

	create_executable("script");
	make_file("script",
			"#!/bin/sh\n"
			"sed 's/get//' < $2 > $2_out\n"
			"mv $2_out $2\n");

	update_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type(cfg.shell);

	curr_stats.exec_env_type = EET_LINUX_NATIVE;
	update_string(&cfg.vi_command, "./script");

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", cwd);
	populate_dir_list(&lwin, /*reload=*/0);

	ui_sb_msg("");

	/* Move to "symlink" file. */
	(void)vle_keys_exec_timed_out(WK_j);
	/* Enter visual mode. */
	(void)vle_keys_exec_timed_out(WK_v);
	/* Change link target. */
	(void)vle_keys_exec_timed_out(WK_c WK_l);

	assert_string_equal("1 link retargeted", ui_sb_last());

	chdir(cwd);

	char target[PATH_MAX + 1];
	assert_success(get_link_target(SANDBOX_PATH "/symlink", target,
				sizeof(target)));
	assert_string_equal("tar", target);

	remove_file(SANDBOX_PATH "/symlink");
	remove_file(SANDBOX_PATH "/script");

	undo_teardown();
	conf_teardown();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
