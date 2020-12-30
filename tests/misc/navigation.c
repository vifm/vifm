#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/marks.h"
#include "../../src/running.h"

SETUP()
{
	char cwd[PATH_MAX + 1];

	curr_view = &lwin;
	other_view = &rwin;

	cfg.fuse_home = strdup("no");
	cfg.slow_fs_list = strdup("");
	cfg.chase_links = 1;

	view_setup(&lwin);
	view_setup(&rwin);

	assert_non_null(get_cwd(cwd, sizeof(cwd)));
	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "..",
			cwd);
}

TEARDOWN()
{
	update_string(&cfg.slow_fs_list, NULL);
	update_string(&cfg.fuse_home, NULL);

	cfg.chase_links = 0;

	view_teardown(&lwin);
	view_teardown(&rwin);
}

TEST(custom_view_is_preserved_on_goto_mark)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(marks_set_user(&lwin, 'a', TEST_DATA_PATH "/existing-files",
				"a"));

	assert_success(marks_goto(&lwin, 'a'));
	assert_true(flist_custom_active(&lwin));
}

TEST(local_filter_is_reset_in_cv_to_follow_mark)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_success(marks_set_user(&lwin, 'a', TEST_DATA_PATH "/existing-files",
				"a"));
	assert_true(flist_custom_active(&lwin));

	local_filter_set(&lwin, "b");
	local_filter_accept(&lwin);
	assert_true(flist_custom_active(&lwin));

	assert_success(marks_goto(&lwin, 'a'));

	assert_true(flist_custom_active(&lwin));
	assert_true(filter_is_empty(&lwin.local_filter.filter));
}

TEST(following_resets_cv)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	rn_follow(&lwin, 0);
	assert_false(flist_custom_active(&lwin));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
