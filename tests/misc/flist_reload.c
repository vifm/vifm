#include <stic.h>

#include <unistd.h> /* chdir() rmdir() */

#include <stddef.h> /* NULL */

#include <test-utils.h>

#include "../../src/ui/ui.h"
#include "../../src/filelist.h"

static view_t *const view = &lwin;

SETUP()
{
	view_setup(view);
	make_abs_path(view->curr_dir, sizeof(view->curr_dir), SANDBOX_PATH, "", NULL);
	assert_success(chdir(view->curr_dir));

	create_dir("0");
	create_dir("1");
	create_dir("2");
	create_dir("3");
	create_dir("4");
	create_dir("5");

	populate_dir_list(view, /*reload=*/0);
}

TEARDOWN()
{
	view_teardown(view);

	/* Some of these files won't exist depending on the last test. */
	(void)rmdir("0");
	(void)rmdir("1");
	(void)rmdir("2");
	(void)rmdir("3");
	(void)rmdir("4");
	(void)rmdir("5");
}

TEST(exact_match_is_preferred)
{
	view->list_pos = 2;
	remove_dir("0");
	remove_dir("1");

	assert_string_equal("2", view->dir_entry[view->list_pos].name);
	populate_dir_list(view, /*reload=*/1);
	assert_string_equal("2", view->dir_entry[view->list_pos].name);
}

TEST(closest_match_below_is_choosen)
{
	view->list_pos = 1;
	remove_dir("1");
	remove_dir("2");

	assert_string_equal("1", view->dir_entry[view->list_pos].name);
	populate_dir_list(view, /*reload=*/1);
	assert_string_equal("3", view->dir_entry[view->list_pos].name);
}

TEST(closest_match_above_is_choosen)
{
	view->list_pos = 5;
	remove_dir("5");

	assert_string_equal("5", view->dir_entry[view->list_pos].name);
	populate_dir_list(view, /*reload=*/1);
	assert_string_equal("4", view->dir_entry[view->list_pos].name);
}

TEST(selection_is_preserved)
{
	view->dir_entry[0].selected = 1;
	view->dir_entry[1].selected = 1;
	view->dir_entry[3].selected = 1;
	view->selected_files = 3;
	remove_dir("3");

	populate_dir_list(view, /*reload=*/1);
	assert_true(view->dir_entry[0].selected);
	assert_true(view->dir_entry[1].selected);
	assert_int_equal(2, view->selected_files);
}

TEST(top_line_is_not_shifted_unnecessarily)
{
	view->window_rows = 4;
	view->top_line = 2;
	view->list_pos = 3;
	remove_dir("0");
	remove_dir("1");

	assert_string_equal("2", view->dir_entry[view->top_line].name);
	assert_string_equal("3", view->dir_entry[view->list_pos].name);

	populate_dir_list(view, /*reload=*/1);

	assert_int_equal(0, view->top_line);
	assert_int_equal(1, view->list_pos);
	assert_string_equal("2", view->dir_entry[view->top_line].name);
	assert_string_equal("3", view->dir_entry[view->list_pos].name);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
