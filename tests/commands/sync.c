#include <stic.h>

#include <unistd.h> /* chdir() rmdir() */

#include <limits.h> /* INT_MAX */
#include <stdio.h> /* remove() */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/cfg/config.h"
#include "../../src/ui/column_view.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"

static void column_line_print(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info);

static char *saved_cwd;

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	cmds_init();

	cfg.slow_fs_list = strdup("");

	view_setup(&lwin);
	view_setup(&rwin);

	lwin.window_rows = 1;
	rwin.window_rows = 1;

	saved_cwd = save_cwd();
}

TEARDOWN()
{
	restore_cwd(saved_cwd);

	vle_cmds_reset();

	free(cfg.slow_fs_list);
	cfg.slow_fs_list = NULL;

	view_teardown(&lwin);
	view_teardown(&rwin);
}

TEST(sync_syncs_local_filter)
{
	other_view->curr_dir[0] = '\0';
	assert_true(change_directory(curr_view, ".") >= 0);
	populate_dir_list(curr_view, 0);
	local_filter_apply(curr_view, "a");

	assert_success(cmds_dispatch("sync! location filters", curr_view,
				CIT_COMMAND));
	assert_string_equal("a", other_view->local_filter.filter.raw);
}

TEST(sync_syncs_filelist)
{
	char cwd[PATH_MAX + 1];

	lwin.window_rows = 1;
	rwin.window_rows = 1;

	opt_handlers_setup();

	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "..", cwd);

	flist_custom_start(curr_view, "test");
	flist_custom_add(curr_view, TEST_DATA_PATH "/existing-files/a");
	flist_custom_add(curr_view, TEST_DATA_PATH "/existing-files/b");
	flist_custom_add(curr_view, TEST_DATA_PATH "/existing-files/c");
	flist_custom_add(curr_view, TEST_DATA_PATH "/rename/a");
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "existing-files", cwd);
	assert_true(flist_custom_finish(curr_view, CV_VERY, 0) == 0);
	curr_view->list_pos = 3;

	assert_success(cmds_dispatch("sync! filelist cursorpos", curr_view,
				CIT_COMMAND));

	assert_true(flist_custom_active(other_view));
	assert_int_equal(curr_view->list_rows, other_view->list_rows);
	assert_int_equal(curr_view->list_pos, other_view->list_pos);

	opt_handlers_teardown();
}

TEST(sync_removes_leafs_and_tree_data_on_converting_tree_to_cv)
{
	lwin.window_rows = 2;
	rwin.window_rows = 2;

	opt_handlers_setup();
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));

	assert_non_null(get_cwd(curr_view->curr_dir, sizeof(curr_view->curr_dir)));
	assert_success(flist_load_tree(curr_view, SANDBOX_PATH, INT_MAX));
	assert_int_equal(2, curr_view->list_rows);

	assert_success(cmds_dispatch("sync! filelist", curr_view, CIT_COMMAND));
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_true(flist_custom_active(other_view));
	assert_int_equal(1, other_view->list_rows);
	assert_int_equal(0, other_view->dir_entry[0].child_count);
	assert_int_equal(0, other_view->dir_entry[0].child_pos);
	assert_int_equal(CV_VERY, other_view->custom.type);

	assert_success(rmdir(SANDBOX_PATH "/dir"));
	opt_handlers_teardown();
}

TEST(sync_syncs_trees)
{
	char cwd[PATH_MAX + 1];

	columns_set_line_print_func(&column_line_print);
	other_view->columns = columns_create();

	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "..", cwd);

	assert_success(flist_load_tree(curr_view, TEST_DATA_PATH "/tree", INT_MAX));

	curr_view->dir_entry[0].selected = 1;
	curr_view->selected_files = 1;
	flist_custom_exclude(curr_view, 1);
	assert_int_equal(0, curr_view->selected_files);

	assert_success(cmds_dispatch("sync! tree", curr_view, CIT_COMMAND));
	assert_true(flist_custom_active(other_view));
	curr_stats.load_stage = 2;
	load_saving_pos(other_view);
	curr_stats.load_stage = 0;

	assert_int_equal(curr_view->list_rows, other_view->list_rows);

	columns_teardown();
}

TEST(sync_all_does_not_turn_destination_into_tree)
{
	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);
	columns_set_line_print_func(&column_line_print);

	opt_handlers_setup();

	other_view->curr_dir[0] = '\0';
	other_view->custom.type = CV_REGULAR;
	other_view->columns = columns_create();

	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	populate_dir_list(curr_view, 0);
	local_filter_apply(curr_view, "a");

	assert_success(cmds_dispatch("sync! all", curr_view, CIT_COMMAND));
	assert_false(other_view->custom.type == CV_TREE);

	opt_handlers_teardown();

	columns_teardown();
}

TEST(sync_localopts_clones_local_options)
{
	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);
	columns_set_line_print_func(&column_line_print);

	lwin.hide_dot = 1;
	lwin.hide_dot_g = 1;
	rwin.hide_dot = 0;
	rwin.hide_dot_g = 0;

	opt_handlers_setup();

	other_view->curr_dir[0] = '\0';
	other_view->custom.type = CV_REGULAR;
	other_view->columns = columns_create();

	assert_true(change_directory(curr_view, SANDBOX_PATH) >= 0);
	populate_dir_list(curr_view, 0);
	local_filter_apply(curr_view, "a");

	assert_success(cmds_dispatch("sync! localopts", curr_view, CIT_COMMAND));
	assert_true(rwin.hide_dot_g);
	assert_true(rwin.hide_dot);

	opt_handlers_teardown();
	columns_teardown();
}

TEST(tree_syncing_applies_properties_of_destination_view)
{
	char cwd[PATH_MAX + 1];

	columns_set_line_print_func(&column_line_print);
	other_view->columns = columns_create();

	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "..", cwd);

	assert_success(flist_load_tree(curr_view, TEST_DATA_PATH "/tree", INT_MAX));

	/* Exclusion. */
	curr_view->dir_entry[0].selected = 1;
	curr_view->selected_files = 1;
	flist_custom_exclude(curr_view, 1);
	assert_int_equal(0, curr_view->selected_files);

	/* Folding. */
	curr_view->list_pos = 0;
	flist_toggle_fold(curr_view);

	/* Local filter (ignored here because we change directory of the other view,
	 * is this a bug?). */
	local_filter_apply(other_view, "d");

	assert_success(cmds_dispatch("sync! tree", curr_view, CIT_COMMAND));
	assert_int_equal(2, other_view->list_rows);
	assert_string_equal("", other_view->local_filter.filter.raw);

	assert_true(flist_custom_active(other_view));
	curr_stats.load_stage = 2;
	load_saving_pos(other_view);
	curr_stats.load_stage = 0;

	assert_int_equal(2, other_view->list_rows);
	assert_string_equal("", other_view->local_filter.filter.raw);

	columns_teardown();
}

static void
column_line_print(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
	/* Do nothing. */
}

TEST(symlinks_in_paths_are_not_resolved, IF(not_windows))
{
	char buf[PATH_MAX + 1];
	char canonic_path[PATH_MAX + 1];

	char src[PATH_MAX + 1], dst[PATH_MAX + 1];
	make_abs_path(src, sizeof(src), TEST_DATA_PATH, "existing-files", saved_cwd);
	make_abs_path(dst, sizeof(dst), SANDBOX_PATH, "dir-link", saved_cwd);
	assert_success(make_symlink(src, dst));

	assert_success(chdir(SANDBOX_PATH "/dir-link"));
	make_abs_path(buf, sizeof(buf), SANDBOX_PATH, "dir-link", saved_cwd);
	to_canonic_path(buf, "/fake-root", curr_view->curr_dir,
			sizeof(curr_view->curr_dir));

	assert_success(cmds_dispatch("sync ../dir-link/..", curr_view, CIT_COMMAND));
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	make_abs_path(buf, sizeof(buf), SANDBOX_PATH, "", saved_cwd);
	to_canonic_path(buf, "/fake-root", canonic_path, sizeof(canonic_path));
	assert_string_equal(canonic_path, other_view->curr_dir);
	assert_success(remove(SANDBOX_PATH "/dir-link"));
}

TEST(incorrect_parameter_causes_error)
{
	assert_failure(cmds_dispatch("sync! nosuchthing", curr_view, CIT_COMMAND));
}

TEST(sync_syncs_custom_trees)
{
	char test_data[PATH_MAX + 1];
	char path[PATH_MAX + 1];

	opt_handlers_setup();

	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);
	columns_set_line_print_func(&column_line_print);
	other_view->columns = columns_create();

	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", saved_cwd);

	flist_custom_start(curr_view, "test");
	snprintf(path, sizeof(path), "%s/%s", test_data, "compare");
	flist_custom_add(curr_view, path);
	snprintf(path, sizeof(path), "%s/%s", test_data, "read");
	flist_custom_add(curr_view, path);
	snprintf(path, sizeof(path), "%s/%s", test_data, "rename");
	flist_custom_add(curr_view, path);
	snprintf(path, sizeof(path), "%s/%s", test_data, "tree");
	flist_custom_add(curr_view, path);
	assert_true(flist_custom_finish(curr_view, CV_REGULAR, 0) == 0);

	assert_success(flist_load_tree(curr_view, test_data, INT_MAX));

	curr_view->dir_entry[0].selected = 1;
	curr_view->selected_files = 1;
	flist_custom_exclude(curr_view, 1);
	assert_int_equal(0, curr_view->selected_files);

	/* As custom trees. */

	assert_success(cmds_dispatch("sync! tree", curr_view, CIT_COMMAND));
	assert_true(flist_custom_active(other_view));
	curr_stats.load_stage = 2;
	load_saving_pos(other_view);
	curr_stats.load_stage = 0;

	assert_int_equal(CV_CUSTOM_TREE, other_view->custom.type);
	assert_int_equal(curr_view->list_rows, other_view->list_rows);

	/* As custom views. */

	assert_success(cmds_dispatch("sync! filelist", curr_view, CIT_COMMAND));
	assert_true(flist_custom_active(other_view));
	assert_int_equal(CV_VERY, other_view->custom.type);

	opt_handlers_teardown();
	columns_teardown();
}

TEST(sync_all_applies_filters_in_trees)
{
	opt_handlers_setup();
	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);
	columns_set_line_print_func(&column_line_print);
	other_view->columns = columns_create();

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", NULL);

	assert_success(cmds_dispatch("set cvoptions=localfilter", curr_view,
				CIT_COMMAND));
	assert_success(cmds_dispatch("tree", curr_view, CIT_COMMAND));
	local_filter_apply(curr_view, "a");
	assert_success(cmds_dispatch("sync! all", curr_view, CIT_COMMAND));

	assert_string_equal("a", other_view->local_filter.filter.raw);

	columns_teardown();
	opt_handlers_teardown();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
