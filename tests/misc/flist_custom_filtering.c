#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/matcher.h"
#include "../../src/utils/str.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"

static char cwd[PATH_MAX + 1];
static char test_data[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(test_data, sizeof(test_data), TEST_DATA_PATH, "", cwd);
}

SETUP()
{
	update_string(&cfg.fuse_home, "no");

	view_setup(&lwin);
	filters_view_reset(&lwin);

	snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/..", test_data);

	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
}

TEARDOWN()
{
	update_string(&cfg.fuse_home, NULL);

	view_teardown(&lwin);
}

TEST(reload_considers_local_filter)
{
	local_filter_apply(&lwin, "b");

	load_dir_list(&lwin, 1);

	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("b", lwin.dir_entry[0].name);
}

TEST(locally_filtered_files_are_not_lost_on_reload)
{
	local_filter_apply(&lwin, "b");

	load_dir_list(&lwin, 1);
	assert_int_equal(1, lwin.filtered);
}

TEST(applying_local_filter_saves_custom_list)
{
	local_filter_apply(&lwin, "b");
	load_dir_list(&lwin, 1);
	assert_int_equal(1, lwin.list_rows);

	local_filter_remove(&lwin);
	load_dir_list(&lwin, 1);
	assert_int_equal(2, lwin.list_rows);
}

TEST(selection_is_lost_when_file_disappears)
{
	lwin.dir_entry[0].selected = 1;

	local_filter_apply(&lwin, "b");
	load_dir_list(&lwin, 1);
	local_filter_apply(&lwin, "");
	load_dir_list(&lwin, 1);

	assert_int_equal(2, lwin.list_rows);
	assert_false(lwin.dir_entry[0].selected);
	assert_int_equal(0, lwin.selected_files);
}

TEST(selection_is_preserved_on_filter_update)
{
	local_filter_apply(&lwin, "b");
	load_dir_list(&lwin, 1);
	lwin.dir_entry[0].selected = 1;
	local_filter_apply(&lwin, "");
	load_dir_list(&lwin, 1);

	assert_int_equal(2, lwin.list_rows);
	assert_true(lwin.dir_entry[1].selected);
	assert_int_equal(1, lwin.selected_files);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
