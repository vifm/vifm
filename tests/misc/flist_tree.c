#include <stic.h>

#include <unistd.h> /* rmdir() symlink() */

#include <stddef.h> /* NULL */
#include <stdlib.h> /* remove() */
#include <string.h> /* memset() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/fileview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/utils/utils.h"
#include "../../src/event_loop.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/marks.h"
#include "../../src/running.h"
#include "../../src/sort.h"
#include "../../src/status.h"

#include "utils.h"

static void verify_tree_node(column_data_t *cdt, int idx,
		const char expected[]);
static void column_line_print(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info);
static int remove_selected(view_t *view, const dir_entry_t *entry, void *arg);

static char cwd[PATH_MAX + 1], test_data[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);
}

SETUP()
{
	conf_setup();
	update_string(&cfg.fuse_home, "no");

	view_setup(&lwin);

	curr_view = &lwin;
	other_view = &lwin;

	columns_set_line_print_func(&column_line_print);

	lwin.columns = columns_create();
}

TEARDOWN()
{
	conf_teardown();
	view_teardown(&lwin);

	columns_set_line_print_func(NULL);
}

TEST(empty_directory_tree_is_created)
{
	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir", 0700));

	assert_success(load_tree(&lwin, SANDBOX_PATH "/empty-dir", cwd));
	assert_int_equal(1, lwin.list_rows);
	validate_tree(&lwin);

	assert_success(rmdir(SANDBOX_PATH "/empty-dir"));
}

TEST(empty_directory_tree_is_created_dotdirs_option)
{
	cfg.dot_dirs |= DD_NONROOT_PARENT;

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(1, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(complex_tree_is_built_correctly)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(12, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(symlinks_are_loaded_as_files, IF(not_windows))
{
	/* symlink() is not available on Windows, but other code is fine. */
#ifndef _WIN32
	assert_success(symlink(TEST_DATA_PATH, SANDBOX_PATH "/link"));
#endif

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(1, lwin.list_rows);
	assert_int_equal(0, lwin.filtered);
	validate_tree(&lwin);

	(void)filter_set(&lwin.local_filter.filter, "a");

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(1, lwin.list_rows);
	assert_int_equal(1, lwin.filtered);
	validate_tree(&lwin);

	assert_success(remove(SANDBOX_PATH "/link"));
}

TEST(reloading_does_not_count_as_location_change)
{
	cfg.cvoptions = CVO_LOCALFILTER;

	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(12, lwin.list_rows);
	validate_tree(&lwin);

	(void)filter_set(&lwin.local_filter.filter, "dir");
	load_view(&lwin);
	assert_int_equal(5, lwin.list_rows);
	validate_tree(&lwin);

	assert_false(filter_is_empty(&lwin.local_filter.filter));

	cfg.cvoptions = 0;
}

TEST(tree_is_reloaded_manually_with_file_updates)
{
	assert_success(os_mkdir(SANDBOX_PATH "/nested-dir", 0700));
	create_file(SANDBOX_PATH "/a");
	create_file(SANDBOX_PATH "/nested-dir/b");

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(3, lwin.list_rows);

	/* On removal of single file in directory, it's replaced with ".." entry. */
	assert_success(remove(SANDBOX_PATH "/nested-dir/b"));

	load_dir_list(&lwin, 1);
	assert_int_equal(3, lwin.list_rows);
	validate_tree(&lwin);

	assert_success(rmdir(SANDBOX_PATH "/nested-dir"));

	load_dir_list(&lwin, 1);
	assert_int_equal(1, lwin.list_rows);
	validate_tree(&lwin);

	assert_success(remove(SANDBOX_PATH "/a"));
}

TEST(tree_is_reloaded_automatically_with_file_updates)
{
	struct stat st;

	create_file(SANDBOX_PATH "/a");
	create_file(SANDBOX_PATH "/b");

	/* Use presumably older timestamp for directory to be changed (we need one
	 * second difference). */
	assert_success(os_lstat(TEST_DATA_PATH, &st));
	clone_attribs(SANDBOX_PATH, TEST_DATA_PATH, &st);

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(2, lwin.list_rows);

	check_if_filelist_has_changed(&lwin);
	ui_view_query_scheduled_event(&lwin);
	assert_success(remove(SANDBOX_PATH "/b"));
	check_if_filelist_has_changed(&lwin);

	curr_stats.load_stage = 2;
	assert_true(process_scheduled_updates_of_view(&lwin));
	curr_stats.load_stage = 0;

	assert_int_equal(1, lwin.list_rows);
	validate_tree(&lwin);

	assert_success(remove(SANDBOX_PATH "/a"));
}

TEST(nested_directory_change_detection)
{
	struct stat st1, st2;

	assert_success(os_mkdir(SANDBOX_PATH "/nested-dir", 0700));
	create_file(SANDBOX_PATH "/nested-dir/a");
	create_file(SANDBOX_PATH "/nested-dir/b");

	/* Use presumably older timestamp for directory to be changed (we need one
	 * second difference). */
	assert_success(os_stat(TEST_DATA_PATH, &st1));
	clone_attribs(SANDBOX_PATH "/nested-dir", TEST_DATA_PATH, &st1);

	assert_success(os_lstat(SANDBOX_PATH "/nested-dir", &st2));
	/* On WINE time stamps aren't really cloned (even though success is
	 * reported). */
	if(not_wine())
	{
		assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
		assert_int_equal(3, lwin.list_rows);

		check_if_filelist_has_changed(&lwin);
		ui_view_query_scheduled_event(&lwin);
		assert_success(remove(SANDBOX_PATH "/nested-dir/b"));
		check_if_filelist_has_changed(&lwin);

		curr_stats.load_stage = 2;
		assert_true(process_scheduled_updates_of_view(&lwin));
		curr_stats.load_stage = 0;

		assert_int_equal(2, lwin.list_rows);
		validate_tree(&lwin);
	}
	else
	{
		assert_success(remove(SANDBOX_PATH "/nested-dir/b"));
	}

	assert_success(remove(SANDBOX_PATH "/nested-dir/a"));
	assert_success(rmdir(SANDBOX_PATH "/nested-dir"));
}

TEST(excluding_dir_in_tree_excludes_its_children)
{
	assert_success(os_mkdir(SANDBOX_PATH "/nested-dir", 0700));
	create_file(SANDBOX_PATH "/nested-dir/a");

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(2, lwin.list_rows);

	lwin.dir_entry[0].selected = 1;
	lwin.selected_files = 1;
	flist_custom_exclude(&lwin, 1);
	validate_tree(&lwin);

	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("..", lwin.dir_entry[0].name);

	assert_success(remove(SANDBOX_PATH "/nested-dir/a"));
	assert_success(rmdir(SANDBOX_PATH "/nested-dir"));
}

TEST(excluding_nested_dir_in_tree_adds_dummy)
{
	assert_success(os_mkdir(SANDBOX_PATH "/nested-dir", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/nested-dir/nested-dir2", 0700));
	create_file(SANDBOX_PATH "/nested-dir/nested-dir2/a");

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(3, lwin.list_rows);

	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 1;
	flist_custom_exclude(&lwin, 1);
	validate_tree(&lwin);

	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(2, lwin.list_rows);
	assert_string_equal("..", lwin.dir_entry[1].name);
	assert_true(lwin.dir_entry[1].child_pos != 0);

	assert_success(remove(SANDBOX_PATH "/nested-dir/nested-dir2/a"));
	assert_success(rmdir(SANDBOX_PATH "/nested-dir/nested-dir2"));
	assert_success(rmdir(SANDBOX_PATH "/nested-dir"));
}

TEST(excluding_nested_dir_in_tree_can_skip_adding_dummy)
{
	cfg.dot_dirs &= ~DD_TREE_LEAFS_PARENT;

	assert_success(os_mkdir(SANDBOX_PATH "/nested-dir", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/nested-dir/nested-dir2", 0700));
	create_file(SANDBOX_PATH "/nested-dir/nested-dir2/a");

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(3, lwin.list_rows);

	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 1;
	flist_custom_exclude(&lwin, 1);
	validate_tree(&lwin);

	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(1, lwin.list_rows);

	assert_success(remove(SANDBOX_PATH "/nested-dir/nested-dir2/a"));
	assert_success(rmdir(SANDBOX_PATH "/nested-dir/nested-dir2"));
	assert_success(rmdir(SANDBOX_PATH "/nested-dir"));
}

TEST(excluding_single_leaf_file_adds_dummy_correctly)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(12, lwin.list_rows);

	lwin.dir_entry[6].selected = 1;
	lwin.selected_files = 1;

	flist_custom_exclude(&lwin, 1);
	validate_tree(&lwin);

	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(12, lwin.list_rows);
}

TEST(excluding_middle_directory_from_chain_adds_dummy_correctly)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(12, lwin.list_rows);

	/* First, hide one of directories to get dir1 -> dir2 -> dir4 -> file3. */

	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 1;

	flist_custom_exclude(&lwin, 1);
	validate_tree(&lwin);

	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(9, lwin.list_rows);

	/* Then exclude dir4. */

	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 1;

	flist_custom_exclude(&lwin, 1);
	validate_tree(&lwin);

	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(8, lwin.list_rows);
}

TEST(excluded_paths_do_not_appear_after_view_reload)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(12, lwin.list_rows);

	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 1;

	flist_custom_exclude(&lwin, 1);
	validate_tree(&lwin);

	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(0, lwin.filtered);
	assert_int_equal(9, lwin.list_rows);

	load_dir_list(&lwin, 1);
	assert_int_equal(0, lwin.filtered);
	assert_int_equal(9, lwin.list_rows);
	validate_tree(&lwin);

	load_dir_list(&lwin, 1);
	assert_int_equal(0, lwin.filtered);
	assert_int_equal(9, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(excluding_all_children_places_a_dummy)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(12, lwin.list_rows);

	lwin.dir_entry[3].selected = 1;
	lwin.dir_entry[4].selected = 1;
	lwin.selected_files = 2;
	lwin.list_pos = 4;

	flist_custom_exclude(&lwin, 1);
	validate_tree(&lwin);

	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(0, lwin.filtered);
	assert_int_equal(3, lwin.list_pos);
	assert_int_equal(11, lwin.list_rows);
}

TEST(zapping_a_tree_reassigns_children)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(12, lwin.list_rows);
	validate_tree(&lwin);

	lwin.dir_entry[0].selected = 1;
	lwin.selected_files = 1;
	assert_int_equal(1, zap_entries(&lwin, lwin.dir_entry, &lwin.list_rows,
				&remove_selected, NULL, 0, 0));
	assert_int_equal(11, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(tree_sorting_considers_structure)
{
	lwin.hide_dot = 1;

	lwin.sort[0] = SK_BY_NAME;
	memset(&lwin.sort[1], SK_NONE, sizeof(lwin.sort) - 1);

	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(10, lwin.list_rows);
	validate_tree(&lwin);

	assert_string_equal("dir1", lwin.dir_entry[0].name);
	assert_string_equal("dir2", lwin.dir_entry[1].name);
	assert_string_equal("dir3", lwin.dir_entry[2].name);
	assert_string_equal("file1", lwin.dir_entry[3].name);
	assert_string_equal("file2", lwin.dir_entry[4].name);
	assert_string_equal("dir4", lwin.dir_entry[5].name);
	assert_string_equal("file3", lwin.dir_entry[6].name);
	assert_string_equal("file4", lwin.dir_entry[7].name);
	assert_string_equal("dir5", lwin.dir_entry[8].name);
	assert_string_equal("file5", lwin.dir_entry[9].name);

	lwin.sort[0] = SK_BY_NAME;
	memset(&lwin.sort[1], SK_NONE, sizeof(lwin.sort) - 1);
	sort_view(&lwin);

	assert_string_equal("dir1", lwin.dir_entry[0].name);
	assert_string_equal("dir2", lwin.dir_entry[1].name);
	assert_string_equal("dir4", lwin.dir_entry[5].name);
	assert_string_equal("file3", lwin.dir_entry[6].name);
	assert_string_equal("dir3", lwin.dir_entry[2].name);
	assert_string_equal("file1", lwin.dir_entry[3].name);
	assert_string_equal("file2", lwin.dir_entry[4].name);
	assert_string_equal("file4", lwin.dir_entry[7].name);
	assert_string_equal("dir5", lwin.dir_entry[8].name);
	assert_string_equal("file5", lwin.dir_entry[9].name);
}

TEST(short_paths_consider_tree_structure)
{
	char name[NAME_MAX + 1];

	memset(&cfg.type_decs, '\0', sizeof(cfg.type_decs));

	(void)filter_set(&lwin.local_filter.filter, "2");
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(2, lwin.list_rows);
	validate_tree(&lwin);

	get_short_path_of(&lwin, &lwin.dir_entry[0], 1, 1, sizeof(name), name);
	assert_string_equal("dir1/dir2", name);
	get_short_path_of(&lwin, &lwin.dir_entry[1], 1, 1, sizeof(name), name);
	assert_string_equal("dir3/file2", name);
}

TEST(tree_prefixes_are_correct)
{
	size_t prefix_len = 0U;
	column_data_t cdt = { .view = &lwin, .prefix_len = &prefix_len };

	memset(&cfg.type_decs, '\0', sizeof(cfg.type_decs));
	lwin.hide_dot = 1;

	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(10, lwin.list_rows);
	validate_tree(&lwin);

	verify_tree_node(&cdt, 0, "dir1");
	verify_tree_node(&cdt, 1, "|-- dir2");
	verify_tree_node(&cdt, 2, "|   |-- dir3");
	verify_tree_node(&cdt, 3, "|   |   |-- file1");
	verify_tree_node(&cdt, 4, "|   |   `-- file2");
	verify_tree_node(&cdt, 5, "|   `-- dir4");
	verify_tree_node(&cdt, 6, "|       `-- file3");
	verify_tree_node(&cdt, 7, "`-- file4");
	verify_tree_node(&cdt, 8, "dir5");
	verify_tree_node(&cdt, 9, "`-- file5");
}

TEST(dotdirs_do_not_mess_up_change_detection)
{
	cfg.dot_dirs |= DD_NONROOT_PARENT;

	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));

	/* Discard results of the first check. */
	check_if_filelist_has_changed(&lwin);
	(void)ui_view_query_scheduled_event(&lwin);

	check_if_filelist_has_changed(&lwin);
	assert_int_equal(UUE_NONE, ui_view_query_scheduled_event(&lwin));
	check_if_filelist_has_changed(&lwin);
	assert_int_equal(UUE_NONE, ui_view_query_scheduled_event(&lwin));
	check_if_filelist_has_changed(&lwin);
	assert_int_equal(UUE_NONE, ui_view_query_scheduled_event(&lwin));
}

TEST(tree_reload_preserves_selection)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));

	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.dir_entry[2].selected = 1;

	lwin.selected_files = 3;

	load_dir_list(&lwin, 1);

	assert_true(lwin.dir_entry[0].selected);
	assert_true(lwin.dir_entry[1].selected);
	assert_true(lwin.dir_entry[2].selected);
	assert_int_equal(3, lwin.selected_files);
}

TEST(marks_unit_is_able_to_find_leafs)
{
	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir", 0700));

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));

	marks_set_special(&lwin, '<', lwin.dir_entry[1].origin,
			lwin.dir_entry[1].name);
	assert_int_equal(1, marks_find_in_view(&lwin, '<'));

	assert_success(rmdir(SANDBOX_PATH "/empty-dir"));
}

TEST(leafs_are_treated_correctly_on_reloading_saving_pos)
{
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/dir/subdir", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/dir/subdir/subsubdir", 0700));
	create_file(SANDBOX_PATH "/dir/file");

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));

	lwin.list_pos = 3;
	create_file(SANDBOX_PATH "/dir/subdir/subsubdir/file");
	load_view(&lwin);
	assert_int_equal(4, lwin.list_pos);

	assert_success(remove(SANDBOX_PATH "/dir/file"));
	assert_success(remove(SANDBOX_PATH "/dir/subdir/subsubdir/file"));
	assert_success(rmdir(SANDBOX_PATH "/dir/subdir/subsubdir"));
	assert_success(rmdir(SANDBOX_PATH "/dir/subdir"));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
}

TEST(cursor_is_set_on_previous_file)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "tree",
			cwd);
	load_dir_list(&lwin, 1);
	assert_int_equal(3, lwin.list_rows);
	lwin.list_pos = 1;

	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(8, lwin.list_pos);
}

TEST(tree_out_of_cv_with_single_element)
{
	char path[PATH_MAX + 1];
	snprintf(path, sizeof(path), "%s/%s", test_data, "existing-files/a");
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(load_tree(&lwin, TEST_DATA_PATH, cwd));
	assert_int_equal(1, lwin.list_rows);
}

TEST(tree_out_of_cv_with_two_elements)
{
	char path[PATH_MAX + 1];

	flist_custom_start(&lwin, "test");
	snprintf(path, sizeof(path), "%s/%s", test_data, "existing-files/a");
	flist_custom_add(&lwin, path);
	snprintf(path, sizeof(path), "%s/%s", test_data, "read/dos-eof");
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(load_tree(&lwin, TEST_DATA_PATH, cwd));
	assert_int_equal(5, lwin.list_rows);
}

TEST(opening_non_toplevel_dummy_is_not_the_same_as_leaving_tree)
{
	create_dir(SANDBOX_PATH "/a");
	create_dir(SANDBOX_PATH "/a/b");

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(3, lwin.list_rows);
	assert_string_equal("..", lwin.dir_entry[2].name);

	char *saved_cwd = save_cwd();

	lwin.list_pos = 2;
	rn_open(&lwin, FHE_NO_RUN);

	restore_cwd(saved_cwd);

	char expected_path[PATH_MAX + 1];
	make_abs_path(expected_path, sizeof(expected_path), SANDBOX_PATH, "a", cwd);

	assert_true(paths_are_same(expected_path, flist_get_dir(&lwin)));

	remove_dir(SANDBOX_PATH "/a/b");
	remove_dir(SANDBOX_PATH "/a");
}

TEST(dummies_of_leaf_directories_can_be_disabled)
{
	create_dir(SANDBOX_PATH "/a");
	create_dir(SANDBOX_PATH "/a/b");

	cfg.dot_dirs &= ~DD_TREE_LEAFS_PARENT;

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(2, lwin.list_rows);
	assert_string_equal("a", lwin.dir_entry[0].name);
	assert_string_equal("b", lwin.dir_entry[1].name);

	remove_dir(SANDBOX_PATH "/a/b");
	remove_dir(SANDBOX_PATH "/a");
}

TEST(full_path_matchers_work_in_trees)
{
	assert_success(replace_matcher(&lwin.manual_filter,
				"{{*/dir[135]/file[145]}}"));
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(9, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(nesting_limit)
{
	assert_success(load_limited_tree(&lwin, TEST_DATA_PATH "/tree", cwd, 0));
	assert_int_equal(3, lwin.list_rows);

	assert_success(load_limited_tree(&lwin, TEST_DATA_PATH "/tree", cwd, 1));
	assert_int_equal(7, lwin.list_rows);

	assert_success(load_limited_tree(&lwin, TEST_DATA_PATH "/tree", cwd, 2));
	assert_int_equal(9, lwin.list_rows);

	assert_success(load_limited_tree(&lwin, TEST_DATA_PATH "/tree", cwd, 3));
	assert_int_equal(12, lwin.list_rows);

	assert_success(load_limited_tree(&lwin, TEST_DATA_PATH "/tree", cwd, 4));
	assert_int_equal(12, lwin.list_rows);
}

static void
verify_tree_node(column_data_t *cdt, int idx, const char expected[])
{
	char name[NAME_MAX + 1];
	cdt->entry = &cdt->view->dir_entry[idx];
	cdt->line_pos = idx;
	const format_info_t info = { .data = cdt, .id = -1 };
	format_name(NULL, sizeof(name), name, &info);
	assert_string_equal(expected, name);
}

static void
column_line_print(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
	/* Do nothing. */
}

static int
remove_selected(view_t *view, const dir_entry_t *entry, void *arg)
{
	return !entry->selected;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
