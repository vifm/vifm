#include <stic.h>

#include <unistd.h> /* chdir() */

#include <limits.h> /* INT_MIN */
#include <string.h> /* memset() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/options.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/flist_sel.h"
#include "../../src/fops_common.h"
#include "../../src/status.h"

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
	view_setup(&rwin);
	modes_init();
	opt_handlers_setup();

	lwin.sort_g[0] = SK_BY_NAME;
	memset(&lwin.sort_g[1], SK_NONE, sizeof(lwin.sort_g) - 1);
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);
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

TEST(folding)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(12, lwin.list_rows);

	(void)vle_keys_exec_timed_out(WK_z WK_x);
	populate_dir_list(&lwin, /*reload=*/1);
	assert_int_equal(5, lwin.list_rows);

	(void)vle_keys_exec_timed_out(WK_z WK_x);
	populate_dir_list(&lwin, /*reload=*/1);
	assert_int_equal(12, lwin.list_rows);

	lwin.list_pos = 4;
	(void)vle_keys_exec_timed_out(WK_z WK_x);
	populate_dir_list(&lwin, /*reload=*/1);
	assert_int_equal(10, lwin.list_rows);
	assert_int_equal(2, lwin.list_pos);
}

TEST(gs_memory_explicit_stash)
{
	/* Load file list. */
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"existing-files", cwd);
	load_dir_list(&lwin, 0);

	/* Make a selection. */
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[2].selected = 1;

	/* Drop selection. */
	flist_sel_stash_if_nonempty(&lwin);

	/* Leave directory. */
	assert_int_equal(0, change_directory(&lwin, ".."));

	/* Return to previous location */
	navigate_back(&lwin);

	/* Restore selection. */
	flist_sel_restore(&lwin, /*reg=*/NULL);

	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[2].selected);
}

TEST(gs_memory_implicit_stash)
{
	/* Load file list. */
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"existing-files", cwd);
	load_dir_list(&lwin, 0);

	/* Make a selection. */
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[2].selected = 1;

	/* Leave directory.  Selection should be saved automatically. */
	assert_int_equal(0, change_directory(&lwin, ".."));

	/* Return to previous location */
	navigate_back(&lwin);

	/* Restore selection. */
	flist_sel_restore(&lwin, /*reg=*/NULL);

	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[2].selected);
}

TEST(swapping_views_loads_options)
{
	assert_success(vle_opts_set("numberwidth=3", OPT_GLOBAL));
	assert_success(vle_opts_set("numberwidth=7", OPT_LOCAL));
	(void)vle_keys_exec_timed_out(WK_C_w WK_x);
	assert_string_equal("0", vle_opts_get("numberwidth", OPT_GLOBAL));
	assert_string_equal("0", vle_opts_get("numberwidth", OPT_LOCAL));
	(void)vle_keys_exec_timed_out(WK_C_w WK_x);
	assert_string_equal("3", vle_opts_get("numberwidth", OPT_GLOBAL));
	assert_string_equal("7", vle_opts_get("numberwidth", OPT_LOCAL));
}

TEST(gf, IF(not_windows))
{
	char dir_path[PATH_MAX + 1];
	make_abs_path(dir_path, sizeof(dir_path), SANDBOX_PATH, "dir", cwd);

	create_dir(SANDBOX_PATH "/dir");
	assert_success(make_symlink("dir", SANDBOX_PATH "/link1"));
	assert_success(make_symlink("link1", SANDBOX_PATH "/link2"));

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
	assert_success(make_symlink("dir", SANDBOX_PATH "/link1"));
	assert_success(make_symlink("link1", SANDBOX_PATH "/link2"));

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

TEST(cl_on_file, IF(not_windows))
{
	conf_setup();
	undo_setup();
	fops_init(&modcline_prompt, NULL);

	assert_success(make_symlink("target", SANDBOX_PATH "/symlink"));

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", cwd);
	populate_dir_list(&lwin, /*reload=*/0);

	/* Start link editing. */
	(void)vle_keys_exec_timed_out(WK_c WK_l);
	/* Edit. */
	(void)vle_keys_exec_timed_out(L"path");
	/* Save. */
	(void)vle_keys_exec_timed_out(WK_CR);

	chdir(cwd);

	char target[PATH_MAX + 1];
	assert_success(get_link_target(SANDBOX_PATH "/symlink", target,
				sizeof(target)));
	assert_string_equal("targetpath", target);

	remove_file(SANDBOX_PATH "/symlink");

	fops_init(NULL, NULL);
	undo_teardown();
	conf_teardown();
}

TEST(cl_multiple_files, IF(not_windows))
{
	conf_setup();
	undo_setup();
	fops_init(&modcline_prompt, NULL);

	create_dir(SANDBOX_PATH "/links");
	assert_success(make_symlink("froma", SANDBOX_PATH "/links/a"));
	assert_success(make_symlink("fromb", SANDBOX_PATH "/links/b"));

	create_executable(SANDBOX_PATH "/script");
	make_file(SANDBOX_PATH "/script",
			"#!/bin/sh\n"
			"sed 's/from/to/' < \"$2\" > \"$2_out\"\n"
			"mv \"$2_out\" \"$2\"\n");

	char vi_cmd[PATH_MAX + 1];
	make_abs_path(vi_cmd, sizeof(vi_cmd), SANDBOX_PATH, "script", NULL);
	update_string(&cfg.vi_command, vi_cmd);

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "links",
			cwd);
	populate_dir_list(&lwin, /*reload=*/0);

	/* Selection should open editor. */
	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 2;
	(void)vle_keys_exec_timed_out(WK_c WK_l);

	char target[PATH_MAX + 1];
	assert_success(get_link_target(SANDBOX_PATH "/links/a", target,
				sizeof(target)));
	assert_string_equal("toa", target);
	assert_success(get_link_target(SANDBOX_PATH "/links/b", target,
				sizeof(target)));
	assert_string_equal("tob", target);

	/* Running cl again should not open editor as there is no selection. */
	(void)vle_keys_exec_timed_out(WK_c WK_l L"suffix" WK_CR);
	assert_success(get_link_target(SANDBOX_PATH "/links/a", target,
				sizeof(target)));
	assert_string_equal("toasuffix", target);

	remove_file(SANDBOX_PATH "/script");
	remove_file(SANDBOX_PATH "/links/a");
	remove_file(SANDBOX_PATH "/links/b");
	remove_dir(SANDBOX_PATH "/links");

	fops_init(NULL, NULL);
	undo_teardown();
	conf_teardown();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
