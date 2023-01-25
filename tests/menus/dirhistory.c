#include <stic.h>

#include <string.h> /* strcpy() strdup() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/menus/dirhistory_menu.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/flist_hist.h"

SETUP()
{
	enum { HISTORY_SIZE = 10 };

	view_setup(&lwin);
	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("lfile0");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];

	view_setup(&rwin);
	rwin.list_rows = 1;
	rwin.list_pos = 0;
	rwin.dir_entry = dynarray_cextend(NULL,
			rwin.list_rows*sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("lfile0");
	rwin.dir_entry[0].origin = &rwin.curr_dir[0];

	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(HISTORY_SIZE);
	cfg_resize_histories(0);

	cfg_resize_histories(HISTORY_SIZE);
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	cfg_resize_histories(0);
}

TEST(history_of_a_view_is_listed_most_recent_first)
{
	int pos;
	strlist_t list;

	flist_hist_setup(&lwin, SANDBOX_PATH, "file1", 0, 1);
	flist_hist_setup(&lwin, TEST_DATA_PATH, "file2", 0, 1);

	list = list_dir_history(&lwin, &pos);

	assert_int_equal(2, list.nitems);
	assert_string_equal(TEST_DATA_PATH, list.items[0]);
	assert_string_equal(SANDBOX_PATH, list.items[1]);

	free_string_array(list.items, list.nitems);
}

TEST(history_does_not_contain_duplicated_entries)
{
	int pos;
	strlist_t list;

	flist_hist_setup(&lwin, SANDBOX_PATH, "file1", 0, 1);
	flist_hist_setup(&lwin, TEST_DATA_PATH, "file2", 0, 1);
	flist_hist_setup(&lwin, SANDBOX_PATH, "file3", 0, 1);
	flist_hist_setup(&lwin, TEST_DATA_PATH, "file4", 0, 1);

	list = list_dir_history(&lwin, &pos);

	assert_int_equal(2, list.nitems);
	assert_string_equal(TEST_DATA_PATH, list.items[0]);
	assert_string_equal(SANDBOX_PATH, list.items[1]);

	free_string_array(list.items, list.nitems);
}

TEST(empty_history_produces_empty_list)
{
	int pos;
	strlist_t list = list_dir_history(&lwin, &pos);

	assert_int_equal(0, list.nitems);

	free_string_array(list.items, list.nitems);
}

TEST(position_of_current_directory_is_determined)
{
	int pos;
	strlist_t list;

	strcpy(lwin.curr_dir, SANDBOX_PATH);

	flist_hist_setup(&lwin, SANDBOX_PATH, "file1", 0, 1);
	flist_hist_setup(&lwin, TEST_DATA_PATH, "file2", 0, 1);

	list = list_dir_history(&lwin, &pos);

	assert_int_equal(pos, 1);

	free_string_array(list.items, list.nitems);
}

TEST(position_of_current_directory_is_determined_in_custom_view)
{
	int pos;
	strlist_t list;

	char cwd[PATH_MAX + 1];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "..",
			cwd);

	flist_hist_setup(&lwin, TEST_DATA_PATH, "file2", 0, 1);
	flist_hist_setup(&lwin, lwin.curr_dir, "file1", 0, 1);
	flist_hist_setup(&lwin, SANDBOX_PATH, "file3", 0, 1);

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	list = list_dir_history(&lwin, &pos);

	assert_int_equal(pos, 0);

	free_string_array(list.items, list.nitems);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
