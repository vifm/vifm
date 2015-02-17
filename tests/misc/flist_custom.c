#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() */

#include "seatest.h"

#include "../../src/ui/ui.h"
#include "../../src/utils/filter.h"
#include "../../src/filelist.h"

static void
setup(void)
{
	lwin.list_rows = 0;
	lwin.list_pos = 0;
	lwin.dir_entry = NULL;
	assert_int_equal(0, filter_init(&lwin.local_filter.filter, 0));

	strcpy(lwin.curr_dir, "/path");
	free(lwin.custom.orig_dir);
	lwin.custom.orig_dir = NULL;
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

static void
teardown(void)
{
	cleanup_view(&lwin);
}

static void
test_empty_list_is_not_accepted(void)
{
	flist_custom_start(&lwin, "test");
	assert_false(flist_custom_finish(&lwin) == 0);
}

static void
test_duplicates_are_not_added(void)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "test-data/existing-files/a");
	flist_custom_add(&lwin, "test-data/existing-files/a");
	assert_true(flist_custom_finish(&lwin) == 0);
	assert_int_equal(1, lwin.list_rows);
}

static void
test_custom_view_replaces_custom_view_fine(void)
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

void
flist_custom_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_empty_list_is_not_accepted);
	run_test(test_duplicates_are_not_added);
	run_test(test_custom_view_replaces_custom_view_fine);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
