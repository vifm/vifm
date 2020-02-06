#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../src/cfg/config.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/matcher.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/marks.h"
#include "../../src/running.h"

#include "utils.h"

static void init_view(view_t *view);
static void free_view(view_t *view);

SETUP()
{
	char cwd[PATH_MAX + 1];

	curr_view = &lwin;
	other_view = &rwin;

	cfg.fuse_home = strdup("no");
	cfg.slow_fs_list = strdup("");
	cfg.chase_links = 1;

	init_view(&lwin);
	init_view(&rwin);

	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "..",
			cwd);
}

TEARDOWN()
{
	update_string(&cfg.slow_fs_list, NULL);
	update_string(&cfg.fuse_home, NULL);

	cfg.chase_links = 0;

	free_view(&lwin);
	free_view(&rwin);
}

static void
init_view(view_t *view)
{
	char *error;

	filter_init(&view->local_filter.filter, 1);
	assert_non_null(view->manual_filter = matcher_alloc("", 0, 0, "", &error));
	filter_init(&view->auto_filter, 1);

	view->dir_entry = NULL;
	view->list_rows = 0;

	view->custom.entry_count = 0;
	view->custom.entries = NULL;

	view->window_rows = 2;
	view->sort[0] = SK_NONE;
	ui_view_sort_list_ensure_well_formed(view, view->sort);
}

static void
free_view(view_t *view)
{
	int i;

	for(i = 0; i < view->list_rows; ++i)
	{
		fentry_free(view, &view->dir_entry[i]);
	}
	dynarray_free(view->dir_entry);

	for(i = 0; i < view->custom.entry_count; ++i)
	{
		fentry_free(view, &view->custom.entries[i]);
	}
	dynarray_free(view->custom.entries);

	filter_dispose(&view->local_filter.filter);
	matcher_free(view->manual_filter);
	view->manual_filter = NULL;
	filter_dispose(&view->auto_filter);
}

TEST(custom_view_is_preserved_on_goto_mark)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	curr_view = &lwin;
	assert_success(set_user_mark('a', TEST_DATA_PATH "/existing-files", "a"));

	assert_success(goto_mark(&lwin, 'a'));
	assert_true(flist_custom_active(&lwin));
}

TEST(local_filter_is_reset_in_cv_to_follow_mark)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	curr_view = &lwin;
	assert_success(set_user_mark('a', TEST_DATA_PATH "/existing-files", "a"));
	assert_true(flist_custom_active(&lwin));

	local_filter_set(&lwin, "b");
	local_filter_accept(&lwin);
	assert_true(flist_custom_active(&lwin));

	assert_success(goto_mark(&lwin, 'a'));

	assert_true(flist_custom_active(&lwin));
	assert_true(filter_is_empty(&lwin.local_filter.filter));
}

TEST(following_resets_cv)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	rn_follow(&lwin);
	assert_false(flist_custom_active(&lwin));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
