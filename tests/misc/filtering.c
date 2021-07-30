#include <stic.h>

#include <limits.h> /* INT_MAX */
#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/filter.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/matcher.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/opt_handlers.h"
#include "../../src/sort.h"
#include "../../src/status.h"

#define assert_hidden(view, name, is_dir) \
	assert_false( \
			filters_file_is_visible(&view, flist_get_dir(&view), name, is_dir, 1))

#define assert_visible(view, name, is_dir) \
	assert_true( \
			filters_file_is_visible(&view, flist_get_dir(&view), name, is_dir, 1))

static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
}

SETUP()
{
	update_string(&cfg.fuse_home, "no");
	update_string(&cfg.slow_fs_list, "");

	cfg.filter_inverted_by_default = 1;

	view_setup(&lwin);

	lwin.list_rows = 7;
	lwin.list_pos = 2;
	strcpy(lwin.curr_dir, "/some/path");
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("with(round)");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("with[square]");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[2].name = strdup("with{curly}");
	lwin.dir_entry[2].origin = &lwin.curr_dir[0];
	lwin.dir_entry[3].name = strdup("with<angle>");
	lwin.dir_entry[3].origin = &lwin.curr_dir[0];
	lwin.dir_entry[4].name = strdup("withSPECS+*^$?|\\");
	lwin.dir_entry[4].origin = &lwin.curr_dir[0];
	lwin.dir_entry[5].name = strdup("with....dots");
	lwin.dir_entry[5].origin = &lwin.curr_dir[0];
	lwin.dir_entry[6].name = strdup("withnonodots");
	lwin.dir_entry[6].origin = &lwin.curr_dir[0];

	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.dir_entry[2].selected = 1;
	lwin.dir_entry[3].selected = 1;
	lwin.dir_entry[4].selected = 1;
	lwin.dir_entry[5].selected = 1;
	lwin.dir_entry[6].selected = 0;
	lwin.selected_files = 6;

	lwin.invert = cfg.filter_inverted_by_default;

	lwin.column_count = 1;

	view_setup(&rwin);

	rwin.list_rows = 8;
	rwin.list_pos = 2;
	rwin.dir_entry = dynarray_cextend(NULL,
			rwin.list_rows*sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("dir1.d");
	rwin.dir_entry[0].origin = &rwin.curr_dir[0];
	rwin.dir_entry[1].name = strdup("dir2.d");
	rwin.dir_entry[1].origin = &rwin.curr_dir[0];
	rwin.dir_entry[2].name = strdup("dir3.d");
	rwin.dir_entry[2].origin = &rwin.curr_dir[0];
	rwin.dir_entry[3].name = strdup("file1.d");
	rwin.dir_entry[3].origin = &rwin.curr_dir[0];
	rwin.dir_entry[4].name = strdup("file2.d");
	rwin.dir_entry[4].origin = &rwin.curr_dir[0];
	rwin.dir_entry[5].name = strdup("file3.d");
	rwin.dir_entry[5].origin = &rwin.curr_dir[0];
	rwin.dir_entry[6].name = strdup("withnonodots");
	rwin.dir_entry[6].origin = &rwin.curr_dir[0];
	rwin.dir_entry[7].name = strdup("somedir");
	rwin.dir_entry[7].origin = &rwin.curr_dir[0];

	rwin.dir_entry[0].selected = 0;
	rwin.dir_entry[1].selected = 0;
	rwin.dir_entry[2].selected = 0;
	rwin.dir_entry[3].selected = 0;
	rwin.dir_entry[4].selected = 0;
	rwin.dir_entry[5].selected = 0;
	rwin.dir_entry[6].selected = 0;
	rwin.dir_entry[7].selected = 0;
	rwin.selected_files = 0;

	rwin.invert = cfg.filter_inverted_by_default;

	rwin.column_count = 1;

	update_string(&lwin.prev_manual_filter, "");
	update_string(&lwin.prev_auto_filter, "");
}

TEARDOWN()
{
	update_string(&cfg.slow_fs_list, NULL);
	update_string(&cfg.fuse_home, NULL);

	view_teardown(&lwin);
	view_teardown(&rwin);

	update_string(&lwin.prev_manual_filter, NULL);
	update_string(&lwin.prev_auto_filter, NULL);
}

TEST(filtering)
{
	assert_int_equal(7, lwin.list_rows);
	name_filters_add_active(&lwin);
	assert_int_equal(1, lwin.list_rows);
	assert_int_equal(0, lwin.selected_files);

	assert_string_equal("withnonodots", lwin.dir_entry[0].name);
	assert_visible(lwin, lwin.dir_entry[0].name, 0);
}

TEST(filtering_file_does_not_filter_dir)
{
	char *const name = strdup(rwin.dir_entry[6].name);

	rwin.dir_entry[6].selected = 1;
	rwin.selected_files = 1;

	assert_int_equal(8, rwin.list_rows);
	name_filters_add_active(&rwin);
	assert_int_equal(7, rwin.list_rows);
	assert_int_equal(0, rwin.selected_files);

	assert_hidden(rwin, name, 0);
	assert_visible(rwin, name, 1);

	free(name);
}

TEST(filtering_dir_does_not_filter_file)
{
	char *const name = strdup(rwin.dir_entry[6].name);

	rwin.dir_entry[6].selected = 1;
	rwin.dir_entry[6].type = FT_DIR;
	rwin.selected_files = 1;

	assert_int_equal(8, rwin.list_rows);
	name_filters_add_active(&rwin);
	assert_int_equal(7, rwin.list_rows);
	assert_int_equal(0, rwin.selected_files);

	assert_hidden(rwin, name, 1);
	assert_visible(rwin, name, 0);

	free(name);
}

TEST(filtering_files_does_not_filter_dirs)
{
	(void)replace_matcher(&rwin.manual_filter, "^.*\\.d$");

	assert_visible(rwin, rwin.dir_entry[0].name, 1);
	assert_visible(rwin, rwin.dir_entry[1].name, 1);
	assert_visible(rwin, rwin.dir_entry[2].name, 1);
	assert_hidden(rwin, rwin.dir_entry[3].name, 0);
	assert_hidden(rwin, rwin.dir_entry[4].name, 0);
	assert_hidden(rwin, rwin.dir_entry[5].name, 0);
	assert_visible(rwin, rwin.dir_entry[6].name, 0);
	assert_visible(rwin, rwin.dir_entry[7].name, 1);
	assert_int_equal(8, rwin.list_rows);
}

TEST(filtering_dirs_does_not_filter_files)
{
	(void)replace_matcher(&rwin.manual_filter, "^.*\\.d/$");

	assert_hidden(rwin, rwin.dir_entry[0].name, 1);
	assert_hidden(rwin, rwin.dir_entry[1].name, 1);
	assert_hidden(rwin, rwin.dir_entry[2].name, 1);
	assert_visible(rwin, rwin.dir_entry[3].name, 0);
	assert_visible(rwin, rwin.dir_entry[4].name, 0);
	assert_visible(rwin, rwin.dir_entry[5].name, 0);
	assert_visible(rwin, rwin.dir_entry[6].name, 0);
	assert_visible(rwin, rwin.dir_entry[7].name, 1);
	assert_int_equal(8, rwin.list_rows);
}

TEST(filtering_files_and_dirs)
{
	(void)replace_matcher(&rwin.manual_filter, "^.*\\.d/?$");

	assert_hidden(rwin, rwin.dir_entry[0].name, 1);
	assert_hidden(rwin, rwin.dir_entry[1].name, 1);
	assert_hidden(rwin, rwin.dir_entry[2].name, 1);
	assert_hidden(rwin, rwin.dir_entry[3].name, 0);
	assert_hidden(rwin, rwin.dir_entry[4].name, 0);
	assert_hidden(rwin, rwin.dir_entry[5].name, 0);
	assert_visible(rwin, rwin.dir_entry[6].name, 0);
	assert_visible(rwin, rwin.dir_entry[7].name, 1);
	assert_int_equal(8, rwin.list_rows);
}

TEST(file_after_directory_is_hidden)
{
	char path[PATH_MAX + 1];

	view_teardown(&lwin);
	view_setup(&lwin);

	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read/very-long-line", cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 1;
	name_filters_add_active(&lwin);

	assert_int_equal(1, lwin.list_rows);
	assert_int_equal(0, lwin.selected_files);
}

TEST(global_local_nature_of_normal_zo)
{
	view_teardown(&lwin);
	view_setup(&lwin);

	view_teardown(&rwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	lwin.hide_dot_g = lwin.hide_dot = 0;
	rwin.hide_dot_g = rwin.hide_dot = 1;

	opt_handlers_setup();
	load_view_options(curr_view);

	init_modes();
	init_commands();

	curr_stats.global_local_settings = 1;

	assert_success(exec_commands("normal zo", &lwin, CIT_COMMAND));
	assert_false(lwin.hide_dot_g);
	assert_false(lwin.hide_dot);
	assert_false(rwin.hide_dot_g);
	assert_false(rwin.hide_dot);

	curr_stats.global_local_settings = 0;

	vle_keys_reset();
	vle_cmds_reset();
	opt_handlers_teardown();
}

TEST(cursor_is_not_moved_from_parent_dir_initially)
{
	char path[PATH_MAX + 1];

	cfg.dot_dirs = DD_NONROOT_PARENT;

	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read/very-long-line", cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	lwin.list_pos = 0;
	assert_int_equal(0, local_filter_set(&lwin, ""));
	local_filter_update_view(&lwin, 0);
	assert_int_equal(0, lwin.list_pos);

	assert_int_equal(0, local_filter_set(&lwin, "l"));
	local_filter_update_view(&lwin, 0);
	assert_int_equal(1, lwin.list_pos);
	local_filter_cancel(&lwin);

	cfg.dot_dirs = 0;
}

TEST(cursor_is_moved_to_nearest_neighbour)
{
	char path[PATH_MAX + 1];

	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read/binary-data", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read/dos-eof", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read/two-lines", cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "read/very-long-line", cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	lwin.list_pos = 1;
	assert_int_equal(0, local_filter_set(&lwin, "l"));
	local_filter_update_view(&lwin, 0);
	assert_int_equal(0, lwin.list_pos);
	local_filter_cancel(&lwin);
}

TEST(removed_filename_filter_is_stored)
{
	assert_success(filter_set(&lwin.auto_filter, "a"));
	assert_success(replace_matcher(&lwin.manual_filter, "b"));

	name_filters_remove(&lwin);

	assert_string_equal("a", lwin.prev_auto_filter);
	assert_string_equal("b", lwin.prev_manual_filter);
}

TEST(filename_filter_can_removed_at_most_once)
{
	assert_success(filter_set(&lwin.auto_filter, "a"));
	assert_success(replace_matcher(&lwin.manual_filter, "b"));

	name_filters_remove(&lwin);
	name_filters_remove(&lwin);

	assert_string_equal("a", lwin.prev_auto_filter);
	assert_string_equal("b", lwin.prev_manual_filter);
}

TEST(filename_filter_can_be_cleared)
{
	assert_success(filter_set(&lwin.auto_filter, "a"));
	assert_success(replace_matcher(&lwin.manual_filter, "b"));

	name_filters_drop(&lwin);

	assert_true(filter_is_empty(&lwin.auto_filter));
	assert_true(matcher_is_empty(lwin.manual_filter));

	assert_true(name_filters_empty(&lwin));
}

TEST(filename_filter_can_be_restored)
{
	assert_success(filter_set(&lwin.auto_filter, "a"));
	assert_success(replace_matcher(&lwin.manual_filter, "b"));

	name_filters_remove(&lwin);
	name_filters_restore(&lwin);

	assert_string_equal("a", lwin.auto_filter.raw);
	assert_string_equal("b", matcher_get_expr(lwin.manual_filter));
}

TEST(filename_filter_is_not_restored_from_empty_state)
{
	assert_success(filter_set(&lwin.auto_filter, "a"));
	assert_success(replace_matcher(&lwin.manual_filter, "b"));

	name_filters_restore(&lwin);

	assert_string_equal("a", lwin.auto_filter.raw);
	assert_string_equal("b", matcher_get_expr(lwin.manual_filter));
}

TEST(filename_filter_can_match_full_paths)
{
	assert_success(replace_matcher(&lwin.manual_filter, "///some/path/b$//"));
	assert_visible(lwin, "a", 0);
	assert_visible(lwin, "a", 0);
	assert_hidden(lwin, "b", 0);
	assert_visible(lwin, "b", 1);

	assert_success(replace_matcher(&lwin.manual_filter, "{{/some/path/b}}"));
	assert_visible(lwin, "a", 0);
	assert_visible(lwin, "a", 0);
	assert_hidden(lwin, "b", 0);
	assert_visible(lwin, "b", 1);

	assert_success(replace_matcher(&lwin.manual_filter, "///other/path/b//"));
	assert_visible(lwin, "b", 0);
	assert_visible(lwin, "b", 1);

	assert_success(replace_matcher(&lwin.manual_filter, "{{/other/path/b}}"));
	assert_visible(lwin, "b", 0);
	assert_visible(lwin, "b", 1);

	assert_success(replace_matcher(&lwin.manual_filter,
				"//^/some/path/[^/]*\\.png$//"));
	assert_hidden(lwin, "a.png", 0);
	assert_visible(lwin, "a.png", 1);
	strcat(lwin.curr_dir, "/nested");
	assert_visible(lwin, "a.png", 0);
	assert_visible(lwin, "a.png", 1);
}

TEST(custom_tree_can_restore_files_after_local_filter_interactive)
{
	char test_data[PATH_MAX + 1];
	char path[PATH_MAX + 1];

	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);

	flist_custom_start(&lwin, "test");
	snprintf(path, sizeof(path), "%s/%s", test_data, "compare");
	flist_custom_add(&lwin, path);
	snprintf(path, sizeof(path), "%s/%s", test_data, "read");
	flist_custom_add(&lwin, path);
	snprintf(path, sizeof(path), "%s/%s", test_data, "rename");
	flist_custom_add(&lwin, path);
	snprintf(path, sizeof(path), "%s/%s", test_data, "tree");
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(flist_load_tree(&lwin, test_data, INT_MAX));
	assert_int_equal(5, lwin.list_rows);

	assert_int_equal(0, local_filter_set(&lwin, "t"));
	local_filter_accept(&lwin);
	assert_int_equal(2, lwin.list_rows);

	assert_int_equal(0, local_filter_set(&lwin, ""));
	local_filter_accept(&lwin);
	assert_int_equal(5, lwin.list_rows);
}

TEST(custom_tree_can_restore_files_after_local_filter_non_interactive)
{
	char test_data[PATH_MAX + 1];
	char path[PATH_MAX + 1];

	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);

	flist_custom_start(&lwin, "test");
	snprintf(path, sizeof(path), "%s/%s", test_data, "compare");
	flist_custom_add(&lwin, path);
	snprintf(path, sizeof(path), "%s/%s", test_data, "read");
	flist_custom_add(&lwin, path);
	snprintf(path, sizeof(path), "%s/%s", test_data, "rename");
	flist_custom_add(&lwin, path);
	snprintf(path, sizeof(path), "%s/%s", test_data, "tree");
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(flist_load_tree(&lwin, test_data, INT_MAX));
	assert_int_equal(5, lwin.list_rows);

	local_filter_apply(&lwin, "t");
	load_dir_list(&lwin, 1);
	assert_int_equal(2, lwin.list_rows);

	local_filter_apply(&lwin, "");
	load_dir_list(&lwin, 1);
	assert_int_equal(5, lwin.list_rows);
}

TEST(sorting_tree_preserves_parent_dot_dir)
{
	char test_data[PATH_MAX + 1];
	char path[PATH_MAX + 1];

	cfg.dot_dirs = DD_NONROOT_PARENT;

	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);

	flist_custom_start(&lwin, "test");
	snprintf(path, sizeof(path), "%s/%s", test_data, "compare");
	flist_custom_add(&lwin, path);
	snprintf(path, sizeof(path), "%s/%s", test_data, "read");
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(flist_load_tree(&lwin, test_data, INT_MAX));
	assert_int_equal(4, lwin.list_rows);

	local_filter_apply(&lwin, "/");
	load_dir_list(&lwin, 1);
	assert_int_equal(4, lwin.list_rows);

	assert_string_equal("..", lwin.dir_entry[0].name);
	sort_view(&lwin);
	assert_string_equal("..", lwin.dir_entry[0].name);

	cfg.dot_dirs = 0;
}

TEST(case_is_ignored_by_noninteractive_local_filter)
{
	cfg.ignore_case = 1;

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", cwd);

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "compare");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	local_filter_apply(&lwin, "CoMpArE");
	load_dir_list(&lwin, 1);

	assert_string_equal("compare", lwin.dir_entry[0].name);

	cfg.ignore_case = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
