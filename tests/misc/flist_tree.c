#include <stic.h>

#include <unistd.h> /* rmdir() symlink() */

#include <stddef.h> /* NULL */
#include <stdlib.h> /* remove() */
#include <string.h> /* memset() */

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/fileview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/utils/utils.h"
#include "../../src/event_loop.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/sort.h"
#include "../../src/status.h"

#include "utils.h"

static void verify_tree_node(column_data_t *cdt, int idx, const char
		expected[]);
static void column_line_print(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align, const char full_column[]);
static int remove_selected(FileView *view, const dir_entry_t *entry, void *arg);
static void validate_tree(const FileView *view);
static void validate_parents(const dir_entry_t *entries, int nchildren);

SETUP()
{
	update_string(&cfg.fuse_home, "no");
	update_string(&cfg.slow_fs_list, "");

	view_setup(&lwin);

	curr_view = &lwin;
	other_view = &lwin;

	columns_set_line_print_func(&column_line_print);
}

TEARDOWN()
{
	update_string(&cfg.slow_fs_list, NULL);
	update_string(&cfg.fuse_home, NULL);

	view_teardown(&lwin);

	columns_set_line_print_func(NULL);
}

TEST(empty_directory_tree_is_created)
{
	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir", 0700));

	assert_success(flist_load_tree(&lwin, SANDBOX_PATH "/empty-dir"));
	assert_int_equal(1, lwin.list_rows);
	validate_tree(&lwin);

	assert_success(rmdir(SANDBOX_PATH "/empty-dir"));
}

TEST(complex_tree_is_built_correctly)
{
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(12, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(symlinks_are_loaded_as_files, IF(not_windows))
{
	/* symlink() is not available on Windows, but other code is fine. */
#ifndef _WIN32
	assert_success(symlink(SANDBOX_PATH, SANDBOX_PATH "/link"));
#endif

	assert_success(flist_load_tree(&lwin, SANDBOX_PATH));
	assert_int_equal(1, lwin.list_rows);
	validate_tree(&lwin);

	assert_success(remove(SANDBOX_PATH "/link"));
}

TEST(tree_accounts_for_dot_filter)
{
	lwin.hide_dot = 1;

	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(10, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(tree_accounts_for_auto_filter)
{
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(12, lwin.list_rows);
	lwin.dir_entry[11].selected = 1;
	lwin.selected_files = 1;
	filter_selected_files(&lwin);

	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(11, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(tree_accounts_for_manual_filter)
{
	(void)filter_set(&lwin.manual_filter, "^\\.hidden$");
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(11, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(tree_accounts_for_local_filter)
{
	(void)filter_set(&lwin.local_filter.filter, "file|dir");
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(10, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(tree_is_reloaded_manually_with_file_updates)
{
	assert_success(os_mkdir(SANDBOX_PATH "/nested-dir", 0700));
	create_file(SANDBOX_PATH "/a");
	create_file(SANDBOX_PATH "/nested-dir/b");

	assert_success(flist_load_tree(&lwin, SANDBOX_PATH));
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

	update_string(&cfg.ruler_format, "");
	lwin.columns = columns_create();

	create_file(SANDBOX_PATH "/a");
	create_file(SANDBOX_PATH "/b");

	/* Use presumably older timestamp for directory to be changed (we need one
	 * second difference). */
	assert_success(os_lstat(TEST_DATA_PATH, &st));
	clone_timestamps(SANDBOX_PATH, TEST_DATA_PATH, &st);

	assert_success(flist_load_tree(&lwin, SANDBOX_PATH));
	assert_int_equal(2, lwin.list_rows);

	check_if_filelist_have_changed(&lwin);
	ui_view_query_scheduled_event(&lwin);
	assert_success(remove(SANDBOX_PATH "/b"));
	check_if_filelist_have_changed(&lwin);

	curr_stats.load_stage = 2;
	assert_true(process_scheduled_updates_of_view(&lwin));
	curr_stats.load_stage = 0;

	assert_int_equal(1, lwin.list_rows);
	validate_tree(&lwin);

	assert_success(remove(SANDBOX_PATH "/a"));

	columns_free(lwin.columns);
	lwin.columns = NULL_COLUMNS;
	update_string(&cfg.ruler_format, NULL);
}

TEST(nested_directory_change_detection)
{
	struct stat st;

	update_string(&cfg.ruler_format, "");
	lwin.columns = columns_create();

	assert_success(os_mkdir(SANDBOX_PATH "/nested-dir", 0700));
	create_file(SANDBOX_PATH "/nested-dir/a");
	create_file(SANDBOX_PATH "/nested-dir/b");

	/* Use presumably older timestamp for directory to be changed (we need one
	 * second difference). */
	assert_success(os_lstat(TEST_DATA_PATH, &st));
	clone_timestamps(SANDBOX_PATH "/nested-dir", TEST_DATA_PATH, &st);

	assert_success(flist_load_tree(&lwin, SANDBOX_PATH));
	assert_int_equal(3, lwin.list_rows);

	check_if_filelist_have_changed(&lwin);
	ui_view_query_scheduled_event(&lwin);
	assert_success(remove(SANDBOX_PATH "/nested-dir/b"));
	check_if_filelist_have_changed(&lwin);

	curr_stats.load_stage = 2;
	assert_true(process_scheduled_updates_of_view(&lwin));
	curr_stats.load_stage = 0;

	assert_int_equal(2, lwin.list_rows);
	validate_tree(&lwin);

	assert_success(remove(SANDBOX_PATH "/nested-dir/a"));
	assert_success(rmdir(SANDBOX_PATH "/nested-dir"));

	columns_free(lwin.columns);
	lwin.columns = NULL_COLUMNS;
	update_string(&cfg.ruler_format, NULL);
}

TEST(excluding_dir_in_tree_excludes_its_children)
{
	assert_success(os_mkdir(SANDBOX_PATH "/nested-dir", 0700));
	create_file(SANDBOX_PATH "/nested-dir/a");

	assert_success(flist_load_tree(&lwin, SANDBOX_PATH));
	assert_int_equal(2, lwin.list_rows);

	lwin.dir_entry[0].selected = 1;
	lwin.selected_files = 1;
	flist_custom_exclude(&lwin);
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

	assert_success(flist_load_tree(&lwin, SANDBOX_PATH));
	assert_int_equal(3, lwin.list_rows);

	lwin.dir_entry[1].selected = 1;
	lwin.selected_files = 1;
	flist_custom_exclude(&lwin);
	validate_tree(&lwin);

	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(2, lwin.list_rows);
	assert_string_equal("..", lwin.dir_entry[1].name);
	assert_true(lwin.dir_entry[1].child_pos != 0);

	assert_success(remove(SANDBOX_PATH "/nested-dir/nested-dir2/a"));
	assert_success(rmdir(SANDBOX_PATH "/nested-dir/nested-dir2"));
	assert_success(rmdir(SANDBOX_PATH "/nested-dir"));
}

TEST(excluding_single_leaf_file_adds_dummy_correctly)
{
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(12, lwin.list_rows);

	lwin.dir_entry[6].selected = 1;
	lwin.selected_files = 1;

	flist_custom_exclude(&lwin);
	validate_tree(&lwin);

	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(12, lwin.list_rows);
}

TEST(excluding_middle_directory_from_chain_adds_dummy_correctly)
{
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(12, lwin.list_rows);

	/* First, hide one of directories to get dir1 -> dir2 -> dir4 -> file3. */

	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 1;

	flist_custom_exclude(&lwin);
	validate_tree(&lwin);

	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(9, lwin.list_rows);

	/* Then exclude dir4. */

	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 1;

	flist_custom_exclude(&lwin);
	validate_tree(&lwin);

	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(8, lwin.list_rows);
}

TEST(local_filter_does_not_block_visiting_directories)
{
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(12, lwin.list_rows);
	validate_tree(&lwin);

	/* Set manual filter to make sure that local filter doesn't dominate it. */
	(void)filter_set(&lwin.manual_filter, "2");

	(void)filter_set(&lwin.local_filter.filter, "file");
	load_dir_list(&lwin, 1);
	assert_int_equal(2, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(dot_filter_dominates_local_filter_in_tree)
{
	/* Set filter to make sure that local filter doesn't dominate them. */
	lwin.hide_dot = 1;

	assert_success(os_mkdir(SANDBOX_PATH "/.nested-dir", 0700));
	create_file(SANDBOX_PATH "/.nested-dir/a");

	assert_success(flist_load_tree(&lwin, SANDBOX_PATH));
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("..", lwin.dir_entry[0].name);
	validate_tree(&lwin);

	(void)filter_set(&lwin.local_filter.filter, "a");
	load_dir_list(&lwin, 1);
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("..", lwin.dir_entry[0].name);
	validate_tree(&lwin);

	assert_success(remove(SANDBOX_PATH "/.nested-dir/a"));
	assert_success(rmdir(SANDBOX_PATH "/.nested-dir"));
}

TEST(non_matching_local_filter_results_in_single_dummy)
{
	(void)filter_set(&lwin.local_filter.filter, "no matches");
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("..", lwin.dir_entry[0].name);
	validate_tree(&lwin);
}

TEST(zapping_a_tree_reassigns_children)
{
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
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

	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
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

TEST(nodes_are_reparented_on_filtering)
{
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(12, lwin.list_rows);
	validate_tree(&lwin);

	assert_int_equal(0, local_filter_set(&lwin, "2"));
	local_filter_accept(&lwin);
	assert_int_equal(2, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(short_paths_consider_tree_structure)
{
	char name[NAME_MAX];

	memset(&cfg.type_decs, '\0', sizeof(cfg.type_decs));

	(void)filter_set(&lwin.local_filter.filter, "2");
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
	assert_int_equal(2, lwin.list_rows);
	validate_tree(&lwin);

	get_short_path_of(&lwin, &lwin.dir_entry[0], 1, sizeof(name), name);
	assert_string_equal("dir1/dir2", name);
	get_short_path_of(&lwin, &lwin.dir_entry[1], 1, sizeof(name), name);
	assert_string_equal("dir3/file2", name);
}

TEST(tree_prefixes_are_correct)
{
	size_t prefix_len = 0U;
	column_data_t cdt = { .view = &lwin, .prefix_len = &prefix_len };

	memset(&cfg.type_decs, '\0', sizeof(cfg.type_decs));
	lwin.hide_dot = 1;

	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));
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
	cfg.dot_dirs = DD_NONROOT_PARENT;

	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));

	/* Discard results of the first check. */
	check_if_filelist_have_changed(&lwin);
	(void)ui_view_query_scheduled_event(&lwin);

	check_if_filelist_have_changed(&lwin);
	assert_int_equal(UUE_NONE, ui_view_query_scheduled_event(&lwin));
	check_if_filelist_have_changed(&lwin);
	assert_int_equal(UUE_NONE, ui_view_query_scheduled_event(&lwin));
	check_if_filelist_have_changed(&lwin);
	assert_int_equal(UUE_NONE, ui_view_query_scheduled_event(&lwin));

	cfg.dot_dirs = 0;
}

TEST(tree_reload_preserves_selection)
{
	assert_success(flist_load_tree(&lwin, TEST_DATA_PATH "/tree"));

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

static void
verify_tree_node(column_data_t *cdt, int idx, const char expected[])
{
	char name[NAME_MAX];
	cdt->line_pos = idx;
	format_name(-1, cdt, sizeof(name), name);
	assert_string_equal(expected, name);
}

static void
column_line_print(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align, const char full_column[])
{
	/* Do nothing. */
}

static int
remove_selected(FileView *view, const dir_entry_t *entry, void *arg)
{
	return !entry->selected;
}

static void
validate_tree(const FileView *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		const dir_entry_t *const e = &view->dir_entry[i];
		assert_true(i + e->child_count + 1 <= view->list_rows);
		assert_true(i - e->child_pos >= 0);

		if(e->child_pos != 0)
		{
			const int j = i - e->child_pos;
			const dir_entry_t *const p = &view->dir_entry[j];
			assert_true(p->child_count >= e->child_pos);
			assert_true(j + p->child_count >= e->child_pos + e->child_count);
		}
	}
	validate_parents(view->dir_entry, view->list_rows);
}

static void
validate_parents(const dir_entry_t *entries, int nchildren)
{
	int i = 0;
	while(i < nchildren)
	{
		const int parent = entries[0].child_pos;
		if(parent == 0)
		{
			assert_int_equal(0, entries[i].child_pos);
		}
		else
		{
			assert_int_equal(parent + i, entries[i].child_pos);
		}
		validate_parents(&entries[i] + 1, entries[i].child_count);
		i += entries[i].child_count + 1;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
