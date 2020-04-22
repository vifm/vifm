#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* strcmp() strcpy() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/dir_stack.h"
#include "../../src/filelist.h"

static void load_view_pair(const char left_path[], const char right_path[]);

static char cwd[PATH_MAX + 1];

SETUP_ONCE()
{
	assert_non_null(get_cwd(cwd, sizeof(cwd)));
}

SETUP()
{
	dir_stack_clear();
	dir_stack_freeze();

	view_setup(&lwin);
	view_setup(&rwin);

	strcpy(lwin.curr_dir, "/left");
	strcpy(rwin.curr_dir, "/right");

	curr_view = &lwin;
	other_view = &rwin;

	opt_handlers_setup();
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	opt_handlers_teardown();
}

TEST(underflow_is_detected)
{
	assert_failure(dir_stack_pop());
}

TEST(pop_returns_views_to_previous_location)
{
	load_view_pair(TEST_DATA_PATH "/read", TEST_DATA_PATH "/rename");
	assert_success(dir_stack_push_current());

	strcpy(lwin.curr_dir, "/new-left");
	strcpy(rwin.curr_dir, "/new-right");

	assert_success(dir_stack_pop());

	assert_false(strcmp(lwin.curr_dir, "/new-left") == 0);
	assert_false(strcmp(rwin.curr_dir, "/new-right") == 0);
}

TEST(swap_swaps_current_locations_and_stack_top)
{
	load_view_pair(TEST_DATA_PATH "/read", TEST_DATA_PATH "/rename");
	assert_success(dir_stack_push_current());
	load_view_pair(TEST_DATA_PATH "/rename", TEST_DATA_PATH "/read");

	assert_success(dir_stack_swap());

	assert_true(ends_with(lwin.curr_dir, "/read"));
	assert_true(ends_with(rwin.curr_dir, "/rename"));

	assert_success(dir_stack_swap());

	assert_true(ends_with(lwin.curr_dir, "/rename"));
	assert_true(ends_with(rwin.curr_dir, "/read"));
}

TEST(empty_stack_is_described_via_current_directories)
{
	char **list = dir_stack_list();
	assert_string_equal("/left", list[0]);
	assert_string_equal("/right", list[1]);
	assert_string_equal(NULL, list[2]);
	free_string_array(list, 3);
}

TEST(non_empty_stack_is_described_correctly)
{
	char **list;

	load_view_pair(TEST_DATA_PATH "/read", TEST_DATA_PATH "/rename");
	assert_success(dir_stack_push_current());
	load_view_pair(TEST_DATA_PATH "/rename", TEST_DATA_PATH "/read");
	assert_success(dir_stack_push_current());

	strcpy(lwin.curr_dir, "/new-left");
	strcpy(rwin.curr_dir, "/new-right");

	list = dir_stack_list();
	assert_string_equal("/new-left", list[0]);
	assert_string_equal("/new-right", list[1]);
	assert_string_equal("-----", list[2]);
	assert_true(ends_with(list[3], "/rename"));
	assert_true(ends_with(list[4], "/read"));
	assert_string_equal("-----", list[5]);
	assert_true(ends_with(list[6], "/read"));
	assert_true(ends_with(list[7], "/rename"));
	assert_string_equal(NULL, list[8]);
	free_string_array(list, 9);
}

TEST(stack_is_changed_by_push_and_successful_pop)
{
	/* Push. */

	assert_false(dir_stack_changed());

	load_view_pair(TEST_DATA_PATH "/read", TEST_DATA_PATH "/rename");

	assert_success(dir_stack_push_current());
	assert_true(dir_stack_changed());

	/* Successful pop. */

	dir_stack_freeze();
	assert_false(dir_stack_changed());

	assert_success(dir_stack_pop());
	assert_true(dir_stack_changed());

	/* Failed pop. */

	dir_stack_freeze();
	assert_false(dir_stack_changed());

	assert_failure(dir_stack_pop());
	assert_false(dir_stack_changed());
}

TEST(rotate_does_nothing_for_zero_argument)
{
	load_view_pair(TEST_DATA_PATH "/read", TEST_DATA_PATH "/rename");
	assert_success(dir_stack_push_current());
	load_view_pair(TEST_DATA_PATH "/rename", TEST_DATA_PATH "/read");

	assert_success(dir_stack_rotate(0));

	assert_true(ends_with(lwin.curr_dir, "/rename"));
	assert_true(ends_with(rwin.curr_dir, "/read"));
}

TEST(rotate_rotates_the_stack)
{
	load_view_pair(TEST_DATA_PATH "/read", TEST_DATA_PATH "/rename");
	assert_success(dir_stack_push_current());
	load_view_pair(TEST_DATA_PATH "/tree", TEST_DATA_PATH "/various-sizes");
	assert_success(dir_stack_push_current());
	load_view_pair(TEST_DATA_PATH "/rename", TEST_DATA_PATH "/read");

	assert_success(dir_stack_rotate(2));
	assert_true(ends_with(lwin.curr_dir, "/read"));
	assert_true(ends_with(rwin.curr_dir, "/rename"));

	assert_success(dir_stack_rotate(2));
	assert_true(ends_with(lwin.curr_dir, "/tree"));
	assert_true(ends_with(rwin.curr_dir, "/various-sizes"));

	assert_success(dir_stack_rotate(2));
	assert_true(ends_with(lwin.curr_dir, "/rename"));
	assert_true(ends_with(rwin.curr_dir, "/read"));

	assert_success(dir_stack_rotate(1));
	assert_true(ends_with(lwin.curr_dir, "/tree"));
	assert_true(ends_with(rwin.curr_dir, "/various-sizes"));
}

static void
load_view_pair(const char left_path[], const char right_path[])
{
	char path[PATH_MAX + 1];

	make_abs_path(path, sizeof(path), left_path, "", cwd);
	to_canonic_path(path, "", lwin.curr_dir, sizeof(lwin.curr_dir));
	populate_dir_list(&lwin, 0);

	make_abs_path(path, sizeof(path), right_path, "", cwd);
	to_canonic_path(path, "", rwin.curr_dir, sizeof(rwin.curr_dir));
	populate_dir_list(&rwin, 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
