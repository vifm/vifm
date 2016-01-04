#include <stic.h>

#include <unistd.h> /* chdir() rmdir() symlink() */

#include <stdio.h> /* fclose() fopen() fprintf() remove() */
#include <stdlib.h> /* free() */
#include <string.h> /* memset() */

#include "../../src/cfg/config.h"
#include "../../src/cfg/info.h"
#include "../../src/cfg/info_chars.h"
#include "../../src/compat/os.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/filter.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/macros.h"
#include "../../src/registers.h"
#include "../../src/running.h"
#include "../../src/sort.h"

#include "utils.h"

static void setup_custom_view(FileView *view);
static int not_windows(void);

SETUP()
{
	update_string(&cfg.fuse_home, "no");
	update_string(&cfg.slow_fs_list, "");

	/* So that nothing is written into directory history. */
	rwin.list_rows = 0;

	view_setup(&lwin);

	curr_view = &lwin;
	other_view = &lwin;
}

TEARDOWN()
{
	update_string(&cfg.slow_fs_list, NULL);
	update_string(&cfg.fuse_home, NULL);

	view_teardown(&lwin);
}

TEST(empty_list_is_not_accepted)
{
	flist_custom_start(&lwin, "test");
	assert_false(flist_custom_finish(&lwin, 0) == 0);
}

TEST(duplicates_are_not_added)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, 0) == 0);
	assert_int_equal(1, lwin.list_rows);
}

TEST(custom_view_replaces_custom_view_fine)
{
	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, 0) == 0);
	assert_int_equal(1, lwin.list_rows);

	assert_true(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	assert_true(flist_custom_finish(&lwin, 0) == 0);
	assert_int_equal(1, lwin.list_rows);

	assert_true(flist_custom_active(&lwin));
}

TEST(reload_considers_local_filter)
{
	filters_view_reset(&lwin);

	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	assert_true(flist_custom_finish(&lwin, 0) == 0);

	local_filter_apply(&lwin, "b");

	load_dir_list(&lwin, 1);

	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("b", lwin.dir_entry[0].name);

	filter_dispose(&lwin.manual_filter);
	filter_dispose(&lwin.auto_filter);
}

TEST(reload_does_not_remove_broken_symlinks, IF(not_windows))
{
	assert_success(chdir(SANDBOX_PATH));

	/* symlink() is not available on Windows, but other code is fine. */
#ifndef _WIN32
	assert_success(symlink("/wrong/path", "broken-link"));
#endif

	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	flist_custom_add(&lwin, "./broken-link");
	assert_true(flist_custom_finish(&lwin, 0) == 0);

	assert_int_equal(2, lwin.list_rows);
	load_dir_list(&lwin, 1);
	assert_int_equal(2, lwin.list_rows);

	assert_success(remove("broken-link"));
}

TEST(locally_filtered_files_are_not_lost_on_reload)
{
	filters_view_reset(&lwin);

	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	assert_true(flist_custom_finish(&lwin, 0) == 0);

	local_filter_apply(&lwin, "b");

	load_dir_list(&lwin, 1);
	assert_int_equal(1, lwin.filtered);

	filter_dispose(&lwin.manual_filter);
	filter_dispose(&lwin.auto_filter);
}

TEST(register_macros_are_expanded_relatively_to_orig_dir)
{
	char *expanded;

	setup_custom_view(&lwin);

	init_registers();

	assert_success(chdir(TEST_DATA_PATH));
	assert_success(append_to_register('r', "existing-files/b"));
	expanded = expand_macros("%rr:p", NULL, NULL, 0);
	assert_string_equal("/path/existing-files/b", expanded);
	free(expanded);

	clear_registers();
}

TEST(dir_macros_are_expanded_to_orig_dir)
{
	char *expanded;

	setup_custom_view(&lwin);

	expanded = expand_macros("%d", NULL, NULL, 0);
	assert_string_equal("/path", expanded);
	free(expanded);

	expanded = expand_macros("%D", NULL, NULL, 0);
	assert_string_equal("/path", expanded);
	free(expanded);
}

TEST(files_are_sorted_undecorated)
{
	assert_success(chdir(SANDBOX_PATH));

	cfg.decorations[FT_DIR][1] = '/';

	lwin.sort[0] = SK_BY_NAME;
	memset(&lwin.sort[1], SK_NONE, sizeof(lwin.sort) - 1);

	assert_success(os_mkdir("foo", 0700));
	assert_success(os_mkdir("foo-", 0700));
	assert_success(os_mkdir("foo0", 0700));

	assert_false(flist_custom_active(&lwin));
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "foo");
	flist_custom_add(&lwin, "foo-");
	flist_custom_add(&lwin, "foo0");
	assert_success(flist_custom_finish(&lwin, 0));

	assert_string_equal("foo", lwin.dir_entry[0].name);
	assert_string_equal("foo-", lwin.dir_entry[1].name);
	assert_string_equal("foo0", lwin.dir_entry[2].name);

	assert_success(rmdir("foo"));
	assert_success(rmdir("foo-"));
	assert_success(rmdir("foo0"));
}

TEST(unsorted_custom_view_does_not_change_order_of_files)
{
	opt_handlers_setup();

	assert_false(flist_custom_active(&lwin));
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, 1) == 0);

	assert_string_equal("b", lwin.dir_entry[0].name);
	assert_string_equal("a", lwin.dir_entry[1].name);

	opt_handlers_teardown();
}

TEST(sorted_custom_view_after_unsorted)
{
	opt_handlers_setup();

	lwin.sort[0] = SK_BY_NAME;
	memset(&lwin.sort[1], SK_NONE, sizeof(lwin.sort) - 1);

	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, 1) == 0);

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, 0) == 0);

	assert_string_equal("a", lwin.dir_entry[0].name);
	assert_string_equal("b", lwin.dir_entry[1].name);

	opt_handlers_teardown();
}

TEST(unsorted_view_remains_one_on_vifminfo_reread_on_restart)
{
	FILE *const f = fopen(SANDBOX_PATH "/vifminfo", "w");
	fprintf(f, "%c%d", LINE_TYPE_LWIN_SORT, SK_BY_NAME);
	fclose(f);

	opt_handlers_setup();

	assert_false(flist_custom_active(&lwin));
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, 1) == 0);
	assert_true(lwin.custom.unsorted);
	assert_int_equal(SK_NONE, lwin.sort[0]);

	/* ls-like view blocks view column updates. */
	copy_str(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH);
	curr_stats.restart_in_progress = 1;
	read_info_file(1);
	curr_stats.restart_in_progress = 0;

	assert_true(lwin.custom.unsorted);
	assert_int_equal(SK_NONE, lwin.sort[0]);

	opt_handlers_teardown();

	assert_success(remove("vifminfo"));
}

TEST(location_is_saved_on_entering_custom_view)
{
	char cwd[PATH_MAX];

	cfg_resize_histories(10);

	/* Set sandbox directory as working directory. */
	assert_success(chdir(TEST_DATA_PATH "/existing-files"));
	assert_true(get_cwd(cwd, sizeof(cwd)) == cwd);
	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), cwd);

	/* Put specific history entry and make sure it's used. */
	save_view_history(&lwin, lwin.curr_dir, "b", 1);
	load_dir_list(&lwin, 0);
	assert_string_equal("b", lwin.dir_entry[lwin.list_pos].name);

	/* Pick different entry. */
	lwin.list_pos = 2;
	assert_string_equal("c", lwin.dir_entry[lwin.list_pos].name);

	/* Go into custom view (load_stage enables saving history). */
	curr_stats.load_stage = 2;
	setup_custom_view(&lwin);
	curr_stats.load_stage = 0;

	/* Return to previous directory and check that last location was used. */
	cd_updir(&lwin, 1);
	assert_string_equal("c", lwin.dir_entry[lwin.list_pos].name);

	cfg_resize_histories(0U);
}

TEST(parent_link_has_correct_origin_field)
{
	/* This directory entry is added separately and thus can stray from the
	 * others. */

	cfg.dot_dirs = DD_NONROOT_PARENT;
	setup_custom_view(&lwin);
	cfg.dot_dirs = 0;

	assert_string_equal("..", lwin.dir_entry[0].name);
	assert_string_equal("/path", lwin.dir_entry[0].origin);
}

static void
setup_custom_view(FileView *view)
{
	assert_false(flist_custom_active(view));
	flist_custom_start(view, "test");
	flist_custom_add(view, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(view, 0) == 0);
}

static int
not_windows(void)
{
#ifdef _WIN32
	return 0;
#else
	return 1;
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
