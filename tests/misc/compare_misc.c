#include <stic.h>

#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/filter.h"
#include "../../src/utils/fs.h"
#include "../../src/cmd_core.h"
#include "../../src/compare.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/flist_pos.h"
#include "../../src/running.h"

/* These tests are about about handling of unusual situations on comparing files
 * and results of operations in compare views. */

static char *saved_cwd;

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);
	view_setup(&rwin);

	opt_handlers_setup();

	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);

	saved_cwd = save_cwd();
}

TEARDOWN()
{
	restore_cwd(saved_cwd);

	columns_teardown();

	view_teardown(&lwin);
	view_teardown(&rwin);

	opt_handlers_teardown();
}

TEST(empty_root_directories_abort_single_comparison)
{
	strcpy(lwin.curr_dir, SANDBOX_PATH);
	compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, CF_NONE);

	assert_false(flist_custom_active(&lwin));
}

TEST(empty_root_directories_abort_dual_comparison)
{
	assert_success(os_mkdir(SANDBOX_PATH "/a", 0777));
	assert_success(os_mkdir(SANDBOX_PATH "/b", 0777));

	strcpy(lwin.curr_dir, SANDBOX_PATH "/a");
	strcpy(rwin.curr_dir, SANDBOX_PATH "/b");

	compare_two_panes(CT_CONTENTS, LT_ALL, CF_SHOW);
	assert_false(flist_custom_active(&lwin));

	assert_success(rmdir(SANDBOX_PATH "/a"));
	assert_success(rmdir(SANDBOX_PATH "/b"));
}

TEST(empty_unique_cv_are_created)
{
	assert_success(os_mkdir(SANDBOX_PATH "/a", 0777));
	assert_success(os_mkdir(SANDBOX_PATH "/b", 0777));

	strcpy(lwin.curr_dir, SANDBOX_PATH "/a");
	strcpy(rwin.curr_dir, SANDBOX_PATH "/b");

	compare_two_panes(CT_CONTENTS, LT_UNIQUE, CF_SHOW);
	assert_true(flist_custom_active(&lwin));
	assert_true(flist_custom_active(&rwin));

	assert_int_equal(1, lwin.list_rows);
	assert_int_equal(1, rwin.list_rows);

	assert_string_equal("..", lwin.dir_entry[0].name);
	assert_string_equal("..", rwin.dir_entry[0].name);

	assert_success(rmdir(SANDBOX_PATH "/a"));
	assert_success(rmdir(SANDBOX_PATH "/b"));
}

TEST(listing_wrong_path_does_nothing)
{
	strcpy(lwin.curr_dir, SANDBOX_PATH "/does-not-exist");
	compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, CF_NONE);

	assert_false(flist_custom_active(&lwin));
}

TEST(files_are_found_recursively)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/tree");
	compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, CF_NONE);

	assert_int_equal(7, lwin.list_rows);
}

TEST(compare_skips_dir_symlinks, IF(not_windows))
{
	assert_success(make_symlink(TEST_DATA_PATH, SANDBOX_PATH "/link"));

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, CF_NONE);
	assert_false(flist_custom_active(&lwin));

	assert_success(remove(SANDBOX_PATH "/link"));
}

TEST(not_available_files_are_ignored, IF(regular_unix_user))
{
	copy_file(TEST_DATA_PATH "/read/utf8-bom", SANDBOX_PATH "/utf8-bom");
	copy_file(TEST_DATA_PATH "/read/utf8-bom", SANDBOX_PATH "/utf8-bom-2");
	assert_success(chmod(SANDBOX_PATH "/utf8-bom", 0000));
	assert_success(chmod(SANDBOX_PATH "/utf8-bom-2", 0000));

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, CF_SKIP_EMPTY);
	assert_false(flist_custom_active(&lwin));

	assert_success(remove(SANDBOX_PATH "/utf8-bom"));
	assert_success(remove(SANDBOX_PATH "/utf8-bom-2"));
}

TEST(two_panes_are_left_in_sync)
{
	strcpy(lwin.curr_dir, SANDBOX_PATH);
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/a");

	compare_two_panes(CT_CONTENTS, LT_ALL, CF_SHOW);
	assert_true(flist_custom_active(&lwin));
	assert_true(flist_custom_active(&rwin));

	rn_leave(&lwin, 1);
	assert_false(flist_custom_active(&lwin));
	assert_false(flist_custom_active(&rwin));
}

TEST(exclude_works_with_entries_or_their_groups)
{
	copy_file(TEST_DATA_PATH "/compare/a/same-content-different-name-1",
			SANDBOX_PATH "/same-content-different-name-1");
	copy_file(TEST_DATA_PATH "/compare/a/same-content-different-name-1",
			SANDBOX_PATH "/same-content-different-name-2");
	copy_file(TEST_DATA_PATH "/compare/a/same-name-same-content",
			SANDBOX_PATH "/same-name-same-content");
	copy_file(TEST_DATA_PATH "/compare/a/same-name-same-content",
			SANDBOX_PATH "/same-name-same-content-2");

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/a");
	compare_two_panes(CT_CONTENTS, LT_ALL, CF_SHOW);
	check_compare_invariants(5);

	/* Does nothing on separator. */
	lwin.selected_files = 1;
	lwin.dir_entry[4].selected = 1;
	flist_custom_exclude(&lwin, 1);
	lwin.dir_entry[4].selected = 0;
	check_compare_invariants(5);
	assert_int_equal(1, lwin.selected_files);

	/* Exclude single file from a group. */
	lwin.selected_files = 1;
	lwin.dir_entry[0].selected = 1;
	flist_custom_exclude(&lwin, 1);
	check_compare_invariants(4);
	assert_int_equal(0, lwin.selected_files);

	/* Exclude the whole group. */
	lwin.selected_files = 1;
	lwin.dir_entry[2].selected = 1;
	flist_custom_exclude(&lwin, 0);
	check_compare_invariants(2);
	assert_int_equal(0, lwin.selected_files);

	/* Exclusion of all files leaves the mode. */
	lwin.selected_files = 1;
	lwin.dir_entry[0].selected = 1;
	flist_custom_exclude(&lwin, 0);
	check_compare_invariants(1);
	assert_int_equal(0, lwin.selected_files);
	rwin.selected_files = 1;
	rwin.dir_entry[0].selected = 1;
	flist_custom_exclude(&rwin, 0);
	assert_int_equal(0, rwin.selected_files);
	assert_false(flist_custom_active(&lwin));
	assert_false(flist_custom_active(&rwin));

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(remove(SANDBOX_PATH "/same-content-different-name-1"));
	assert_success(remove(SANDBOX_PATH "/same-content-different-name-2"));
	assert_success(remove(SANDBOX_PATH "/same-name-same-content"));
	assert_success(remove(SANDBOX_PATH "/same-name-same-content-2"));
}

TEST(local_filter_is_not_set)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_two_panes(CT_NAME, LT_ALL, CF_SHOW);

	cmds_dispatch1("f", &lwin, CIT_FILTER_PATTERN);
	assert_true(filter_is_empty(&lwin.local_filter.filter));

	modcline_enter(CLS_FILTER, lwin.local_filter.filter.raw);
	assert_true(vle_mode_is(NORMAL_MODE));
}

TEST(removed_files_disappear_in_both_views_on_reload)
{
	copy_file(TEST_DATA_PATH "/compare/a/same-content-different-name-1",
			SANDBOX_PATH "/same-content-different-name-1");
	copy_file(TEST_DATA_PATH "/compare/a/same-content-different-name-1",
			SANDBOX_PATH "/same-content-different-name-2");
	copy_file(TEST_DATA_PATH "/compare/a/same-name-same-content",
			SANDBOX_PATH "/same-name-same-content");
	copy_file(TEST_DATA_PATH "/compare/a/same-name-same-content",
			SANDBOX_PATH "/same-name-same-content-2");

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/a");
	compare_two_panes(CT_CONTENTS, LT_ALL, CF_SHOW);
	check_compare_invariants(5);

	assert_success(remove(SANDBOX_PATH "/same-content-different-name-1"));
	load_dir_list(&lwin, 1);
	check_compare_invariants(4);
	assert_string_equal("same-content-different-name-1", rwin.dir_entry[0].name);

	assert_success(remove(SANDBOX_PATH "/same-content-different-name-2"));
	load_dir_list(&lwin, 1);
	check_compare_invariants(4);
	assert_string_equal("", lwin.dir_entry[0].name);

	assert_success(remove(SANDBOX_PATH "/same-name-same-content"));
	assert_success(remove(SANDBOX_PATH "/same-name-same-content-2"));
}

TEST(comparison_views_are_closed_when_no_files_are_left)
{
	assert_success(os_mkdir(SANDBOX_PATH "/a", 0777));
	assert_success(os_mkdir(SANDBOX_PATH "/b", 0777));

	copy_file(TEST_DATA_PATH "/compare/a/same-content-different-name-1",
			SANDBOX_PATH "/a/same-content-different-name-1");
	copy_file(TEST_DATA_PATH "/compare/a/same-content-different-name-1",
			SANDBOX_PATH "/b/same-content-different-name-1");

	strcpy(lwin.curr_dir, SANDBOX_PATH "/a");
	strcpy(rwin.curr_dir, SANDBOX_PATH "/b");
	compare_two_panes(CT_CONTENTS, LT_ALL, CF_SHOW);
	check_compare_invariants(1);

	assert_success(remove(SANDBOX_PATH "/a/same-content-different-name-1"));
	load_dir_list(&lwin, 1);
	check_compare_invariants(1);
	assert_string_equal("", lwin.dir_entry[0].name);

	assert_success(remove(SANDBOX_PATH "/b/same-content-different-name-1"));
	load_dir_list(&lwin, 1);
	assert_false(flist_custom_active(&lwin));
	assert_false(flist_custom_active(&rwin));

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(rmdir(SANDBOX_PATH "/a"));
	assert_success(rmdir(SANDBOX_PATH "/b"));
}

TEST(sorting_is_not_changed)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, CF_NONE);

	change_sort_type(&lwin, SK_BY_SIZE, 0);
	assert_int_equal(SK_NONE, lwin.sort[0]);

	cmds_init();
	assert_success(cmds_dispatch1("set sort=ext", &lwin, CIT_COMMAND));
	assert_int_equal(SK_NONE, lwin.sort[0]);
	vle_cmds_reset();
}

TEST(cursor_moves_in_both_views_synchronously)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_two_panes(CT_NAME, LT_ALL, CF_SHOW);

	assert_int_equal(0, lwin.list_pos);
	assert_int_equal(lwin.list_pos, rwin.list_pos);

	fpos_set_pos(&lwin, 2);
	assert_int_equal(2, lwin.list_pos);
	assert_int_equal(lwin.list_pos, rwin.list_pos);

	fpos_set_pos(&rwin, 3);
	assert_int_equal(3, rwin.list_pos);
	assert_int_equal(lwin.list_pos, rwin.list_pos);
}

TEST(two_independent_compare_views_are_not_bound)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, CF_NONE);
	compare_one_pane(&rwin, CT_CONTENTS, LT_ALL, CF_NONE);

	assert_int_equal(0, lwin.list_pos);
	assert_int_equal(lwin.list_pos, rwin.list_pos);

	fpos_set_pos(&lwin, 2);
	fpos_set_pos(&rwin, 3);
	assert_int_equal(2, lwin.list_pos);
	assert_int_equal(3, rwin.list_pos);

	rn_leave(&lwin, 1);
	assert_true(flist_custom_active(&rwin));
}

TEST(diff_is_closed_by_single_compare)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_two_panes(CT_NAME, LT_ALL, CF_SHOW);

	assert_int_equal(CV_DIFF, lwin.custom.type);
	assert_int_equal(CV_DIFF, rwin.custom.type);

	compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, CF_NONE);
	assert_int_equal(CV_COMPARE, lwin.custom.type);
	assert_int_equal(CV_REGULAR, rwin.custom.type);
}

TEST(filtering_fake_entry_does_nothing)
{
	curr_view = &rwin;
	other_view = &lwin;
	strcpy(lwin.curr_dir, SANDBOX_PATH);
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_two_panes(CT_CONTENTS, LT_ALL, CF_GROUP_PATHS | CF_SHOW);

	assert_int_equal(4, lwin.list_rows);
	assert_int_equal(4, rwin.list_rows);
	assert_string_equal("", lwin.dir_entry[0].name);

	lwin.dir_entry[0].selected = 1;
	lwin.selected_files = 1;
	name_filters_add_active(&lwin);

	assert_int_equal(4, lwin.list_rows);
	assert_int_equal(1, lwin.selected_files);
	assert_int_equal(4, rwin.list_rows);
	assert_string_equal("", lwin.dir_entry[0].name);
}

TEST(filtering_updates_two_bound_views)
{
	curr_view = &rwin;
	other_view = &lwin;
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_two_panes(CT_CONTENTS, LT_ALL, CF_SHOW);

	/* Check that single file is excluded. */

	check_compare_invariants(5);
	assert_string_equal("same-content-different-name-1", lwin.dir_entry[0].name);
	assert_string_equal("same-content-different-name-1", rwin.dir_entry[0].name);

	rwin.dir_entry[0].selected = 1;
	rwin.selected_files = 1;
	name_filters_add_active(&rwin);
	assert_int_equal(0, rwin.selected_files);

	check_compare_invariants(4);
	assert_string_equal("same-content-different-name-1", lwin.dir_entry[0].name);
	assert_string_equal("same-content-different-name-2", rwin.dir_entry[0].name);

	/* Check that compare view is left when lists are empty. */

	rwin.dir_entry[0].selected = 1;
	rwin.dir_entry[1].selected = 1;
	rwin.dir_entry[2].selected = 1;
	rwin.selected_files = 3;
	name_filters_add_active(&rwin);
	assert_int_equal(0, rwin.selected_files);
	check_compare_invariants(3);

	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[1].selected = 1;
	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 3;
	name_filters_add_active(&lwin);
	assert_int_equal(0, lwin.selected_files);
	assert_int_equal(CV_REGULAR, lwin.custom.type);
	assert_int_equal(CV_REGULAR, rwin.custom.type);
}

TEST(two_pane_dups_renumbering)
{
	copy_file(TEST_DATA_PATH "/read/utf8-bom", SANDBOX_PATH "/utf8-bom-1");
	copy_file(TEST_DATA_PATH "/read/utf8-bom", SANDBOX_PATH "/utf8-bom-2");

	curr_view = &rwin;
	other_view = &lwin;
	strcpy(lwin.curr_dir, SANDBOX_PATH);
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/read");
	compare_two_panes(CT_CONTENTS, LT_DUPS, CF_SHOW);

	check_compare_invariants(2);

	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(1, lwin.dir_entry[1].id);

	assert_success(remove(SANDBOX_PATH "/utf8-bom-1"));
	assert_success(remove(SANDBOX_PATH "/utf8-bom-2"));
}

TEST(removing_all_files_of_same_id_and_fake_entry_on_the_other_side)
{
	copy_file(TEST_DATA_PATH "/read/binary-data", SANDBOX_PATH "/binary-data");
	copy_file(TEST_DATA_PATH "/read/dos-eof", SANDBOX_PATH "/dos-eof-1");
	copy_file(TEST_DATA_PATH "/read/dos-eof", SANDBOX_PATH "/dos-eof-2");

	strcpy(lwin.curr_dir, SANDBOX_PATH);
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/read");
	compare_two_panes(CT_CONTENTS, LT_ALL, CF_SHOW);

	check_compare_invariants(7);

	assert_success(remove(SANDBOX_PATH "/dos-eof-1"));
	assert_success(remove(SANDBOX_PATH "/dos-eof-2"));
	load_dir_list(&lwin, 1);

	check_compare_invariants(6);
	assert_int_equal(1, lwin.dir_entry[0].id);
	assert_int_equal(2, lwin.dir_entry[1].id);
	assert_int_equal(3, lwin.dir_entry[2].id);
	assert_int_equal(4, lwin.dir_entry[3].id);
	assert_int_equal(5, lwin.dir_entry[4].id);
	assert_int_equal(6, lwin.dir_entry[5].id);

	assert_success(remove(SANDBOX_PATH "/binary-data"));
}

TEST(compare_considers_dot_filter)
{
	lwin.hide_dot = 1;
	rwin.hide_dot = 1;
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/tree");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/tree/dir5");
	compare_two_panes(CT_CONTENTS, LT_ALL, CF_GROUP_PATHS | CF_SHOW);
	assert_int_equal(6, lwin.list_rows);
	assert_int_equal(6, rwin.list_rows);
}

TEST(compare_considers_name_filters)
{
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"/compare/a", saved_cwd);
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), TEST_DATA_PATH,
			"/compare/b", saved_cwd);

	load_dir_list(&lwin, 1);
	assert_string_equal("same-content-different-name-1", lwin.dir_entry[0].name);
	name_filters_add_active(&lwin);
	assert_int_equal(2, lwin.list_rows);

	cmds_dispatch1("different-content", &rwin, CIT_FILTER_PATTERN);
	load_dir_list(&rwin, 1);
	assert_int_equal(1, rwin.list_rows);

	compare_two_panes(CT_CONTENTS, LT_ALL, CF_SHOW);

	check_compare_invariants(3);

	assert_string_equal("same-name-different-content", lwin.dir_entry[0].name);
	assert_string_equal("same-name-same-content", lwin.dir_entry[1].name);
	assert_string_equal("", lwin.dir_entry[2].name);

	assert_string_equal("", rwin.dir_entry[0].name);
	assert_string_equal("", rwin.dir_entry[1].name);
	assert_string_equal("same-name-different-content", rwin.dir_entry[2].name);
}

TEST(empty_files_are_skipped_if_requested)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	compare_two_panes(CT_CONTENTS, LT_ALL, CF_SKIP_EMPTY | CF_SHOW);
	check_compare_invariants(4);
}

TEST(custom_views_are_compared)
{
	char path[PATH_MAX + 1];

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH,
			"/compare/a", saved_cwd);
	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH,
			"compare/a/same-content-different-name-1", saved_cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH,
			"compare/a/same-name-different-content", saved_cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH,
			"compare/a/same-name-same-content", saved_cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");

	compare_two_panes(CT_NAME, LT_ALL, CF_GROUP_PATHS | CF_SHOW);

	check_compare_invariants(4);

	assert_string_equal("same-content-different-name-1", lwin.dir_entry[0].name);
	assert_string_equal("same-content-different-name-1", rwin.dir_entry[0].name);
	assert_string_equal("", lwin.dir_entry[1].name);
	assert_string_equal("same-content-different-name-2", rwin.dir_entry[1].name);
	assert_string_equal("same-name-different-content", lwin.dir_entry[2].name);
	assert_string_equal("same-name-different-content", rwin.dir_entry[2].name);
	assert_string_equal("same-name-same-content", lwin.dir_entry[3].name);
	assert_string_equal("same-name-same-content", rwin.dir_entry[3].name);
}

TEST(directories_are_not_added_from_custom_views)
{
	char path[PATH_MAX + 1];

	strcpy(lwin.curr_dir, "no-such-path");
	flist_custom_start(&lwin, "test");
	make_abs_path(path, sizeof(path), TEST_DATA_PATH,
			"compare/a/same-content-different-name-1", saved_cwd);
	flist_custom_add(&lwin, path);
	make_abs_path(path, sizeof(path), TEST_DATA_PATH, "compare/a/", saved_cwd);
	flist_custom_add(&lwin, path);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	strcpy(rwin.curr_dir, SANDBOX_PATH);

	compare_two_panes(CT_NAME, LT_ALL, CF_GROUP_PATHS | CF_SHOW);

	check_compare_invariants(1);

	assert_string_equal("same-content-different-name-1", lwin.dir_entry[0].name);
	assert_string_equal("", rwin.dir_entry[0].name);
}

TEST(the_same_directories_are_not_compared)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare");

	compare_two_panes(CT_CONTENTS, LT_ALL, CF_SHOW);
	assert_false(flist_custom_active(&lwin));
	assert_false(flist_custom_active(&rwin));
}

/* Results should be the same regardless of which pane is active. */
TEST(two_panes_unique_is_symmetric)
{
	create_dir(SANDBOX_PATH "/a");
	create_dir(SANDBOX_PATH "/b");
	create_file(SANDBOX_PATH "/a/filea");
	create_file(SANDBOX_PATH "/a/fileb");
	create_file(SANDBOX_PATH "/b/filea");

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "a",
			saved_cwd);
	make_abs_path(rwin.curr_dir, sizeof(rwin.curr_dir), SANDBOX_PATH, "b",
			saved_cwd);

	curr_view = &lwin;
	other_view = &rwin;
	compare_two_panes(CT_NAME, LT_UNIQUE, CF_SHOW);
	assert_int_equal(1, lwin.list_rows);
	assert_int_equal(1, rwin.list_rows);
	assert_string_equal("fileb", lwin.dir_entry[0].name);
	assert_string_equal("..", rwin.dir_entry[0].name);

	rn_leave(&lwin, 1);
	rn_leave(&rwin, 1);

	curr_view = &rwin;
	other_view = &lwin;
	compare_two_panes(CT_NAME, LT_UNIQUE, CF_SHOW);
	assert_int_equal(1, lwin.list_rows);
	assert_int_equal(1, rwin.list_rows);
	assert_string_equal("fileb", lwin.dir_entry[0].name);
	assert_string_equal("..", rwin.dir_entry[0].name);

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	remove_file(SANDBOX_PATH "/a/filea");
	remove_file(SANDBOX_PATH "/a/fileb");
	remove_file(SANDBOX_PATH "/b/filea");
	remove_dir(SANDBOX_PATH "/a");
	remove_dir(SANDBOX_PATH "/b");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
