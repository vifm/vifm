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
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/sort.h"

#include "utils.h"

static void column_line_print(const char buf[], size_t offset, AlignType align,
		const char full_column[], const format_info_t *info);

static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
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

	columns_free(lwin.columns);
	lwin.columns = NULL;
}

TEST(tree_accounts_for_dot_filter)
{
	lwin.hide_dot = 1;

	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(10, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(tree_accounts_for_auto_filter)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(12, lwin.list_rows);
	assert_int_equal(0, lwin.filtered);
	lwin.dir_entry[11].selected = 1;
	lwin.selected_files = 1;
	name_filters_add_active(&lwin);
	assert_int_equal(1, lwin.filtered);

	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(11, lwin.list_rows);
	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(1, lwin.filtered);
	validate_tree(&lwin);
}

TEST(tree_accounts_for_manual_filter)
{
	(void)replace_matcher(&lwin.manual_filter, "^\\.hidden$");
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(11, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(tree_accounts_for_local_filter)
{
	(void)filter_set(&lwin.local_filter.filter, "file|dir");
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(10, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(leafs_are_suppressed_by_local_filtering)
{
	(void)filter_set(&lwin.local_filter.filter, "dir");
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(5, lwin.list_rows);
	validate_tree(&lwin);

	/* ".." shouldn't appear after reload. */
	load_view(&lwin);
	assert_int_equal(5, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(leafs_are_not_matched_by_local_filtering)
{
	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir-1", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir-2", 0700));

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(4, lwin.list_rows);

	assert_int_equal(1, local_filter_set(&lwin, "\\."));
	local_filter_accept(&lwin);

	assert_int_equal(1, lwin.list_rows);
	validate_tree(&lwin);

	/* ".." shouldn't appear after reload. */
	load_view(&lwin);
	assert_int_equal(1, lwin.list_rows);
	validate_tree(&lwin);

	assert_success(rmdir(SANDBOX_PATH "/empty-dir-2"));
	assert_success(rmdir(SANDBOX_PATH "/empty-dir-1"));
}

TEST(leafs_are_returned_if_local_filter_is_emptied)
{
	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir-1", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir-2", 0700));

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(4, lwin.list_rows);
	assert_int_equal(0, local_filter_set(&lwin, "."));
	local_filter_accept(&lwin);

	assert_int_equal(2, lwin.list_rows);
	validate_tree(&lwin);

	/* Reload file list to make sure that its state is "clear" (leafs aren't
	 * loaded in this case). */
	load_view(&lwin);

	/* ".." should appear after filter is emptied. */
	assert_int_equal(0, local_filter_set(&lwin, ""));
	local_filter_accept(&lwin);
	assert_int_equal(4, lwin.list_rows);
	validate_tree(&lwin);

	assert_success(rmdir(SANDBOX_PATH "/empty-dir-2"));
	assert_success(rmdir(SANDBOX_PATH "/empty-dir-1"));
}

TEST(local_filter_does_not_block_visiting_directories)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(12, lwin.list_rows);
	validate_tree(&lwin);

	/* Set manual filter to make sure that local filter doesn't dominate it. */
	(void)replace_matcher(&lwin.manual_filter, "2");

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

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
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
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("..", lwin.dir_entry[0].name);
	validate_tree(&lwin);
}

TEST(nodes_are_reparented_on_filtering)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(12, lwin.list_rows);
	validate_tree(&lwin);

	assert_int_equal(0, local_filter_set(&lwin, "2"));
	local_filter_accept(&lwin);
	assert_int_equal(2, lwin.list_rows);
	validate_tree(&lwin);
}

TEST(sorting_of_filtered_list_accounts_for_tree)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(12, lwin.list_rows);
	validate_tree(&lwin);

	assert_int_equal(0, local_filter_set(&lwin, "file|dir4"));
	local_filter_accept(&lwin);
	assert_int_equal(6, lwin.list_rows);
	validate_tree(&lwin);

	sort_view(&lwin);

	assert_string_equal("file1", lwin.dir_entry[0].name);
	assert_string_equal("file2", lwin.dir_entry[1].name);
	assert_string_equal("dir4", lwin.dir_entry[2].name);
	assert_string_equal("file3", lwin.dir_entry[3].name);
	assert_string_equal("file4", lwin.dir_entry[4].name);
	assert_string_equal("file5", lwin.dir_entry[5].name);
}

TEST(filtering_does_not_break_the_tree_with_empty_dir)
{
	cfg.dot_dirs |= DD_NONROOT_PARENT;
	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir", 0700));

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(3, lwin.list_rows);
	validate_tree(&lwin);

	assert_int_equal(0, local_filter_set(&lwin, ""));
	assert_int_equal(3, lwin.list_rows);
	validate_tree(&lwin);
	local_filter_cancel(&lwin);

	assert_success(rmdir(SANDBOX_PATH "/empty-dir"));
}

TEST(filtering_does_not_confuse_leafs_with_parent_ref)
{
	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir", 0700));

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(2, lwin.list_rows);
	validate_tree(&lwin);

	assert_int_equal(1, local_filter_set(&lwin, "g"));
	assert_int_equal(1, lwin.list_rows);
	validate_tree(&lwin);
	local_filter_cancel(&lwin);

	assert_success(rmdir(SANDBOX_PATH "/empty-dir"));
}

TEST(filtering_does_not_hide_parent_refs)
{
	assert_success(os_mkdir(SANDBOX_PATH "/empty-dir", 0700));

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(2, lwin.list_rows);
	validate_tree(&lwin);

	assert_int_equal(0, local_filter_set(&lwin, ""));
	assert_int_equal(2, lwin.list_rows);
	validate_tree(&lwin);
	local_filter_cancel(&lwin);

	assert_success(rmdir(SANDBOX_PATH "/empty-dir"));
}

TEST(filtering_and_nesting_limit)
{
	(void)filter_set(&lwin.local_filter.filter, "^[^1]+$");

	assert_success(load_limited_tree(&lwin, TEST_DATA_PATH "/tree", cwd, 0));
	assert_int_equal(2, lwin.list_rows);
	populate_dir_list(&lwin, /*reload=*/1);
	assert_int_equal(2, lwin.list_rows);
}

static void
column_line_print(const char buf[], size_t offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
	/* Do nothing. */
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
