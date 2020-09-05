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
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

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

	lwin.sort_g[0] = SK_BY_NAME;
	memset(&lwin.sort_g[1], SK_NONE, sizeof(lwin.sort_g) - 1);
}

TEARDOWN()
{
	view_teardown(&lwin);
	vle_keys_reset();
	opt_handlers_teardown();
}

TEST(sibl_navigate_correctly)
{
	char path[PATH_MAX + 1];

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "read",
			cwd);

	(void)vle_keys_exec_timed_out(WK_RB WK_r);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "rename", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	(void)vle_keys_exec_timed_out(WK_LB WK_r);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	(void)vle_keys_exec_timed_out(L"2" WK_RB WK_r);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "scripts", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	(void)vle_keys_exec_timed_out(L"3" WK_LB WK_r);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "quotes-in-names", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	(void)vle_keys_exec_timed_out(L"3" WK_LB WK_r);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "color-schemes", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	(void)vle_keys_exec_timed_out(WK_LB WK_R);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "various-sizes", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));

	(void)vle_keys_exec_timed_out(WK_RB WK_R);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "color-schemes", cwd);
	assert_true(paths_are_same(lwin.curr_dir, path));
}

TEST(center_splitter)
{
	cfg.lines = INT_MIN;
	cfg.columns = INT_MIN;

	stats_set_splitter_pos(10);

	(void)vle_keys_exec_timed_out(WK_C_w WK_EQUALS);

	assert_int_equal(-1, curr_stats.splitter_pos);
	assert_true(curr_stats.splitter_ratio == 0.5);
}

TEST(gf, IF(not_windows))
{
	char dir_path[PATH_MAX + 1];
	make_abs_path(dir_path, sizeof(dir_path), SANDBOX_PATH, "dir", cwd);

	create_dir(SANDBOX_PATH "/dir");
	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	assert_success(symlink("dir", SANDBOX_PATH "/link1"));
	assert_success(symlink("link1", SANDBOX_PATH "/link2"));
#endif

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", cwd);
	load_dir_list(&lwin, 0);

	lwin.list_pos = 2;
	assert_string_equal("link2", get_current_file_name(&lwin));
	(void)vle_keys_exec_timed_out(WK_g WK_f);
	assert_string_equal("link1", get_current_file_name(&lwin));
	(void)vle_keys_exec_timed_out(WK_g WK_f);
	assert_string_equal("dir", get_current_file_name(&lwin));
	(void)vle_keys_exec_timed_out(WK_g WK_f);
	assert_true(paths_are_same(dir_path, lwin.curr_dir));

	chdir(cwd);
	remove_file(SANDBOX_PATH "/link2");
	remove_file(SANDBOX_PATH "/link1");
	remove_dir(SANDBOX_PATH "/dir");
}

TEST(gF, IF(not_windows))
{
	char dir_path[PATH_MAX + 1];
	make_abs_path(dir_path, sizeof(dir_path), SANDBOX_PATH, "dir", cwd);

	create_dir(SANDBOX_PATH "/dir");
	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	assert_success(symlink("dir", SANDBOX_PATH "/link1"));
	assert_success(symlink("link1", SANDBOX_PATH "/link2"));
#endif

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", cwd);
	load_dir_list(&lwin, 0);

	lwin.list_pos = 2;
	assert_string_equal("link2", get_current_file_name(&lwin));
	(void)vle_keys_exec_timed_out(WK_g WK_F);
	assert_string_equal("dir", get_current_file_name(&lwin));
	(void)vle_keys_exec_timed_out(WK_g WK_F);
	assert_true(paths_are_same(dir_path, lwin.curr_dir));

	chdir(cwd);
	remove_file(SANDBOX_PATH "/link2");
	remove_file(SANDBOX_PATH "/link1");
	remove_dir(SANDBOX_PATH "/dir");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
