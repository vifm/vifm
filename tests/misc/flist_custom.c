#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/filter.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"

static void cleanup_view(FileView *view);

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
		free(view->dir_entry[i].name);
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

	local_filter_set(&lwin, "b");

	load_dir_list(&lwin, 1);

	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("b", lwin.dir_entry[0].name);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
