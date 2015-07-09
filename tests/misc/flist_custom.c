#include <stic.h>

#include <unistd.h> /* chdir() rmdir() */

#include <stdlib.h> /* free() */
#include <string.h> /* memset() strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/filter.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/macros.h"
#include "../../src/registers.h"
#include "../../src/sort.h"

static void cleanup_view(FileView *view);
static void setup_custom_view(FileView *view);

SETUP()
{
	cfg.fuse_home = strdup("no");
	lwin.list_rows = 0;
	lwin.filtered = 0;
	lwin.list_pos = 0;
	lwin.dir_entry = NULL;
	assert_int_equal(0, filter_init(&lwin.local_filter.filter, 0));

	strcpy(lwin.curr_dir, "/path");
	free(lwin.custom.orig_dir);
	lwin.custom.orig_dir = NULL;

	curr_view = &lwin;
	other_view = &lwin;
}

TEARDOWN()
{
	free(cfg.fuse_home);
	cfg.fuse_home = NULL;

	cleanup_view(&lwin);
}

static void
cleanup_view(FileView *view)
{
	int i;

	for(i = 0; i < view->list_rows; ++i)
	{
		free_dir_entry(view, &view->dir_entry[i]);
	}
	free(view->dir_entry);

	filter_dispose(&lwin.local_filter.filter);
}

TEST(empty_list_is_not_accepted)
{
	flist_custom_start(&lwin, "test");
	assert_false(flist_custom_finish(&lwin) == 0);
}

TEST(duplicates_are_not_added)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "test-data/existing-files/a");
	flist_custom_add(&lwin, "test-data/existing-files/a");
	assert_true(flist_custom_finish(&lwin) == 0);
	assert_int_equal(1, lwin.list_rows);
}

TEST(custom_view_replaces_custom_view_fine)
{
	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "test-data/existing-files/a");
	assert_true(flist_custom_finish(&lwin) == 0);
	assert_int_equal(1, lwin.list_rows);

	assert_true(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "test-data/existing-files/b");
	assert_true(flist_custom_finish(&lwin) == 0);
	assert_int_equal(1, lwin.list_rows);

	assert_true(flist_custom_active(&lwin));
}

TEST(reload_considers_local_filter)
{
	filters_view_reset(&lwin);

	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "test-data/existing-files/a");
	flist_custom_add(&lwin, "test-data/existing-files/b");
	assert_true(flist_custom_finish(&lwin) == 0);

	local_filter_apply(&lwin, "b");

	load_dir_list(&lwin, 1);

	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("b", lwin.dir_entry[0].name);
}

TEST(locally_filtered_files_are_not_lost_on_reload)
{
	filters_view_reset(&lwin);

	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "test-data/existing-files/a");
	flist_custom_add(&lwin, "test-data/existing-files/b");
	assert_true(flist_custom_finish(&lwin) == 0);

	local_filter_apply(&lwin, "b");

	load_dir_list(&lwin, 1);
	assert_int_equal(1, lwin.filtered);
}

TEST(register_macros_are_expanded_relatively_to_orig_dir)
{
	char *expanded;

	setup_custom_view(&lwin);

	init_registers();

	assert_success(append_to_register('r', "test-data/existing-files/b"));
	expanded = expand_macros("%rr:p", NULL, NULL, 0);
	assert_string_equal("/path/test-data/existing-files/b", expanded);
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
	assert_success(chdir("test-data/sandbox"));

	cfg.decorations[FT_DIR][1] = '/';

	lwin.custom.unsorted = 0;
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
	assert_success(flist_custom_finish(&lwin));

	assert_string_equal("foo", lwin.dir_entry[0].name);
	assert_string_equal("foo-", lwin.dir_entry[1].name);
	assert_string_equal("foo0", lwin.dir_entry[2].name);

	assert_success(rmdir("foo"));
	assert_success(rmdir("foo-"));
	assert_success(rmdir("foo0"));

	assert_success(chdir("../.."));
}

static void
setup_custom_view(FileView *view)
{
	assert_false(flist_custom_active(view));
	flist_custom_start(view, "test");
	flist_custom_add(view, "test-data/existing-files/a");
	assert_true(flist_custom_finish(view) == 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
