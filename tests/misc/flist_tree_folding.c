#include <stic.h>

#include <stdarg.h> /* va_list va_start() va_arg() va_end() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/event_loop.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/sort.h"

#include "utils.h"

static void column_line_print(const char buf[], size_t offset, AlignType align,
		const char full_column[], const format_info_t *info);
static int build_custom_view(view_t *view, ...);
static void toggle_fold_and_update(view_t *view);

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
	lwin.columns = columns_create();

	columns_set_line_print_func(&column_line_print);
}

TEARDOWN()
{
	conf_teardown();
	view_teardown(&lwin);

	columns_set_line_print_func(NULL);
}

TEST(no_folding_in_non_cv)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", cwd);
	populate_dir_list(&lwin, /*reload=*/1);
	assert_int_equal(11, lwin.list_rows);

	flist_toggle_fold(&lwin);
	assert_int_equal(11, lwin.list_rows);
	populate_dir_list(&lwin, /*reload=*/1);
	assert_int_equal(11, lwin.list_rows);
}

TEST(no_folding_for_non_dirs)
{
	assert_true(build_custom_view(&lwin,
				"tree/dir1/dir2",
				"tree/dir1/file4",
				(const char *)NULL) == 0);

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(3, lwin.list_rows);

	lwin.list_pos = 2;
	assert_string_equal("file4", lwin.dir_entry[lwin.list_pos].name);

	flist_toggle_fold(&lwin);
	assert_int_equal(3, lwin.list_rows);
	populate_dir_list(&lwin, /*reload=*/1);
	assert_int_equal(3, lwin.list_rows);
}

TEST(folding_of_directories)
{
	assert_success(os_mkdir(SANDBOX_PATH "/nested-dir", 0700));
	create_file(SANDBOX_PATH "/nested-dir/a");

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(2, lwin.list_rows);

	toggle_fold_and_update(&lwin);

	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("nested-dir", lwin.dir_entry[0].name);
	assert_true(lwin.dir_entry[0].folded);

	toggle_fold_and_update(&lwin);

	assert_int_equal(2, lwin.list_rows);
	assert_string_equal("nested-dir", lwin.dir_entry[0].name);
	assert_string_equal("a", lwin.dir_entry[1].name);
	assert_false(lwin.dir_entry[0].folded);

	assert_success(remove(SANDBOX_PATH "/nested-dir/a"));
	assert_success(rmdir(SANDBOX_PATH "/nested-dir"));
}

TEST(folding_two_tree_out_of_cv)
{
	assert_true(build_custom_view(&lwin,
				"tree/dir1/dir2/dir4",
				"tree/dir1/dir2/dir4/file3",
				(const char *)NULL) == 0);

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(2, lwin.list_rows);

	toggle_fold_and_update(&lwin);
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("dir4", lwin.dir_entry[0].name);
	assert_true(lwin.dir_entry[0].folded);

	toggle_fold_and_update(&lwin);
	assert_int_equal(2, lwin.list_rows);
	assert_string_equal("dir4", lwin.dir_entry[0].name);
	assert_string_equal("file3", lwin.dir_entry[1].name);
	assert_false(lwin.dir_entry[0].folded);
}

TEST(unfolding_accounts_for_sorting)
{
	assert_true(build_custom_view(&lwin,
				"tree/dir1/dir2/dir3",
				"tree/dir1/dir2/dir3/file1",
				"tree/dir1/dir2/dir3/file2",
				(const char *)NULL) == 0);

	assert_success(load_tree(&lwin, SANDBOX_PATH, cwd));
	assert_int_equal(3, lwin.list_rows);
	assert_string_equal("dir3", lwin.dir_entry[0].name);
	assert_string_equal("file1", lwin.dir_entry[1].name);
	assert_string_equal("file2", lwin.dir_entry[2].name);

	toggle_fold_and_update(&lwin);
	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("dir3", lwin.dir_entry[0].name);
	assert_true(lwin.dir_entry[0].folded);

	lwin.sort[0] = -SK_BY_NAME;
	sort_view(&lwin);

	toggle_fold_and_update(&lwin);
	assert_int_equal(3, lwin.list_rows);
	assert_string_equal("dir3", lwin.dir_entry[0].name);
	assert_string_equal("file2", lwin.dir_entry[1].name);
	assert_string_equal("file1", lwin.dir_entry[2].name);
	assert_false(lwin.dir_entry[0].folded);
}

TEST(folding_five_tree_out_of_cv)
{
	assert_true(build_custom_view(&lwin,
				"tree/dir1/dir2/dir3",
				"tree/dir1/dir2/dir3/file1",
				"tree/dir1/dir2/dir3/file2",
				"tree/dir1/dir2/dir4",
				"tree/dir1/dir2/dir4/file3",
				(const char *)NULL) == 0);

	assert_success(load_tree(&lwin, TEST_DATA_PATH, cwd));
	assert_int_equal(6, lwin.list_rows);

	assert_string_equal("dir2", lwin.dir_entry[0].name);
	assert_string_equal("dir3", lwin.dir_entry[1].name);
	assert_string_equal("file1", lwin.dir_entry[2].name);
	assert_string_equal("file2", lwin.dir_entry[3].name);
	assert_string_equal("dir4", lwin.dir_entry[4].name);
	assert_string_equal("file3", lwin.dir_entry[5].name);

	lwin.list_pos = 1;
	toggle_fold_and_update(&lwin);
	assert_int_equal(4, lwin.list_rows);

	toggle_fold_and_update(&lwin);
	assert_int_equal(6, lwin.list_rows);
}

TEST(folds_of_custom_tree_are_not_lost_on_filtering)
{
	assert_true(build_custom_view(&lwin,
				"tree/dir1/dir2/dir3",
				"tree/dir1/dir2/dir3/file1",
				"tree/dir1/dir2/dir3/file2",
				"tree/dir1/dir2/dir4",
				"tree/dir1/dir2/dir4/file3",
				(const char *)NULL) == 0);

	assert_success(load_tree(&lwin, TEST_DATA_PATH, cwd));
	assert_int_equal(6, lwin.list_rows);

	/* fold */
	lwin.list_pos = 1;
	assert_string_equal("dir3", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 2;
	assert_string_equal("dir4", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(3, lwin.list_rows);

	/* filter */
	assert_int_equal(0, local_filter_set(&lwin, "[34]"));
	local_filter_accept(&lwin);
	assert_int_equal(2, lwin.list_rows);

	/* unfold */
	lwin.list_pos = 0;
	assert_string_equal("dir3", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);

	/* remove filter */
	local_filter_remove(&lwin);
	curr_stats.load_stage = 2;
	assert_true(process_scheduled_updates_of_view(&lwin));
	curr_stats.load_stage = 0;
	assert_int_equal(5, lwin.list_rows);
}

TEST(local_filter_respects_tree_folds)
{
	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree/dir1/dir2", cwd));
	assert_int_equal(5, lwin.list_rows);

	lwin.list_pos = 0;
	assert_string_equal("dir3", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(3, lwin.list_rows);

	local_filter_apply(&lwin, "2");
	populate_dir_list(&lwin, /*reload=*/1);

	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("..", lwin.dir_entry[0].name);
}

TEST(local_filter_respects_custom_tree_folds)
{
	assert_true(build_custom_view(&lwin,
				"tree/dir1",
				"tree/dir1/file4",
				(const char *)NULL) == 0);

	assert_success(load_tree(&lwin, TEST_DATA_PATH, cwd));
	assert_int_equal(2, lwin.list_rows);

	lwin.list_pos = 0;
	assert_string_equal("dir1", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(1, lwin.list_rows);

	local_filter_apply(&lwin, "4");
	populate_dir_list(&lwin, /*reload=*/1);

	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("..", lwin.dir_entry[0].name);
}

TEST(folding_parent_does_not_mess_up_children)
{
	assert_true(build_custom_view(&lwin,
				"tree/dir1",
				"tree/dir1/file4",
				"tree/dir5",
				"tree/dir5/file5",
				(const char *)NULL) == 0);

	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(5, lwin.list_rows);

	lwin.list_pos = 1;
	assert_string_equal("dir1", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(4, lwin.list_rows);

	lwin.list_pos = 1;
	assert_string_equal("dir1", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(5, lwin.list_rows);

	lwin.list_pos = 0;
	assert_string_equal("tree", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(1, lwin.list_rows);

	lwin.list_pos = 0;
	assert_string_equal("tree", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(5, lwin.list_rows);

	lwin.list_pos = 1;
	assert_string_equal("dir1", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(4, lwin.list_rows);

	lwin.list_pos = 1;
	assert_string_equal("dir1", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(5, lwin.list_rows);
}

/* This test mixes different trees and does reloading to verify resource uses
 * and tree reloading. */
TEST(folding_grind)
{
	assert_true(build_custom_view(&lwin,
				"tree/dir1",
				"tree/dir1/file4",
				"tree/dir1/dir2",
				"tree/dir1/dir2/dir3",
				"tree/dir1/dir2/dir3/file1",
				"tree/dir1/dir2/dir3/file2",
				"tree/dir1/dir2/dir4",
				"tree/dir1/dir2/dir4/file3",
				"tree/dir5",
				"tree/dir5/.nested_hidden",
				"tree/dir5/file5",
				"tree/.hidden",
				(const char *)NULL) == 0);

	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(CV_CUSTOM_TREE, lwin.custom.type);
	assert_int_equal(13, lwin.list_rows);

	lwin.list_pos = 3;
	assert_string_equal("dir3", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 4;
	assert_string_equal("dir4", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 2;
	assert_string_equal("dir2", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 4;
	assert_string_equal("dir5", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);

	assert_int_equal(6, lwin.list_rows);

	lwin.list_pos = 2;
	assert_string_equal("dir2", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);

	assert_int_equal(8, lwin.list_rows);

	/* Not a custom tree below. */

	assert_success(load_tree(&lwin, TEST_DATA_PATH "/tree", cwd));
	assert_int_equal(CV_TREE, lwin.custom.type);
	assert_int_equal(12, lwin.list_rows);

	lwin.list_pos = 2;
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 3;
	assert_string_equal("dir4", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 1;
	assert_string_equal("dir2", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	lwin.list_pos = 3;
	assert_string_equal("dir5", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);

	assert_int_equal(5, lwin.list_rows);

	lwin.list_pos = 1;
	assert_string_equal("dir2", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);

	assert_int_equal(7, lwin.list_rows);
}

TEST(lazy_unfolding)
{
	assert_success(load_limited_tree(&lwin, TEST_DATA_PATH "/tree", cwd, 0));
	assert_int_equal(3, lwin.list_rows);

	lwin.list_pos = 0;
	assert_string_equal("dir1", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(5, lwin.list_rows);

	lwin.list_pos = 1;
	assert_string_equal("dir2", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(7, lwin.list_rows);

	lwin.list_pos = 1;
	assert_string_equal("dir2", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(5, lwin.list_rows);

	lwin.list_pos = 1;
	assert_string_equal("dir2", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(7, lwin.list_rows);

	lwin.list_pos = 0;
	assert_string_equal("dir1", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(3, lwin.list_rows);

	lwin.list_pos = 0;
	assert_string_equal("dir1", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(7, lwin.list_rows);
}

TEST(lazy_unfolding_and_filtering)
{
	(void)filter_set(&lwin.local_filter.filter, "^[^1]+$");

	assert_success(load_limited_tree(&lwin, TEST_DATA_PATH "/tree", cwd, 0));
	assert_int_equal(2, lwin.list_rows);

	lwin.list_pos = 0;
	assert_string_equal("dir5", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(4, lwin.list_rows);

	lwin.list_pos = 0;
	assert_string_equal("dir5", lwin.dir_entry[lwin.list_pos].name);
	toggle_fold_and_update(&lwin);
	assert_int_equal(2, lwin.list_rows);
}

static void
column_line_print(const char buf[], size_t offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
	/* Do nothing. */
}

static int
build_custom_view(view_t *view, ...)
{
	va_list ap;
	va_start(ap, view);

	flist_custom_start(view, "test");

	const char *rel_path;
	while((rel_path = va_arg(ap, const char *)) != NULL)
	{
		char path[PATH_MAX + 1];
		make_abs_path(path, sizeof(path), TEST_DATA_PATH, rel_path, cwd);
		flist_custom_add(view, path);
	}

	va_end(ap);

	return flist_custom_finish(view, CV_REGULAR, 0);
}

static void
toggle_fold_and_update(view_t *view)
{
	flist_toggle_fold(view);

	validate_tree(&lwin);

	curr_stats.load_stage = 2;
	assert_true(process_scheduled_updates_of_view(view));
	curr_stats.load_stage = 0;

	validate_tree(&lwin);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
