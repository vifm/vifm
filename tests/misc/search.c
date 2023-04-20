#include <stic.h>

#include <unistd.h> /* chdir() */

#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/modes/normal.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/search.h"

static void set_pos_in_curr_view(int pos);

static char *saved_cwd;

SETUP_ONCE()
{
	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN_ONCE()
{
	curr_view = NULL;
	other_view = NULL;
}

SETUP()
{
	saved_cwd = save_cwd();

	view_setup(&lwin);
	view_setup(&rwin);

	assert_success(chdir(TEST_DATA_PATH "/read"));
	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));

	populate_dir_list(&lwin, 0);
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	restore_cwd(saved_cwd);
}

TEST(matches_can_be_highlighted)
{
	cfg.hl_search = 1;

	search_pattern(&lwin, "dos", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_int_equal(2, lwin.matches);
	assert_string_equal("dos-eof", lwin.dir_entry[1].name);
	assert_true(lwin.dir_entry[1].selected);
	assert_string_equal("dos-line-endings", lwin.dir_entry[2].name);
	assert_true(lwin.dir_entry[2].selected);

	cfg.hl_search = 0;
}

TEST(nothing_happens_for_empty_pattern)
{
	search_pattern(&lwin, "", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_int_equal(0, lwin.matches);
}

TEST(wrong_pattern_results_in_no_matches)
{
	search_pattern(&lwin, "asdfasdfasdf", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_int_equal(0, lwin.matches);

	search_pattern(&lwin, "*", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_int_equal(0, lwin.matches);
}

TEST(search_finds_matching_files_and_numbers_them)
{
	search_pattern(&lwin, "dos", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_int_equal(2, lwin.matches);
	assert_string_equal("dos-eof", lwin.dir_entry[1].name);
	assert_int_equal(1, lwin.dir_entry[1].search_match);
	assert_string_equal("dos-line-endings", lwin.dir_entry[2].name);
	assert_int_equal(2, lwin.dir_entry[2].search_match);
}

TEST(cursor_can_be_positioned_on_first_match)
{
	lwin.list_pos = 0;
	search_pattern(&lwin, "dos", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_string_equal("dos-eof", lwin.dir_entry[1].name);
	lwin.list_pos = 1;
}

TEST(reset_clears_counter_and_match_numbers)
{
	int i;

	search_pattern(&lwin, "dos", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_int_equal(2, lwin.matches);

	reset_search_results(&lwin);

	assert_int_equal(0, lwin.matches);

	for(i = 0; i < lwin.list_rows; ++i)
	{
		assert_int_equal(0, lwin.dir_entry[i].search_match);
	}
}

TEST(match_navigation_in_empty_view)
{
	view_teardown(&lwin);
	view_setup(&lwin);

	assert_int_equal(0, lwin.list_rows);
	assert_false(goto_search_match(&lwin, /*backward=*/1, /*count=*/1,
				&set_pos_in_curr_view));
	assert_false(goto_search_match(&lwin, /*backward=*/0, /*count=*/1,
				&set_pos_in_curr_view));
}

TEST(match_navigation_with_wrapping)
{
	cfg.wrap_scan = 1;

	search_pattern(&lwin, "dos", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_int_equal(2, lwin.matches);

	lwin.list_pos = 0;

	/* Forward. */
	goto_search_match(&lwin, /*backward=*/0, /*count=*/1, &set_pos_in_curr_view);
	assert_int_equal(1, lwin.list_pos);
	goto_search_match(&lwin, /*backward=*/0, /*count=*/1, &set_pos_in_curr_view);
	assert_int_equal(2, lwin.list_pos);
	goto_search_match(&lwin, /*backward=*/0, /*count=*/1, &set_pos_in_curr_view);
	assert_int_equal(1, lwin.list_pos);

	/* Backward. */
	goto_search_match(&lwin, /*backward=*/1, /*count=*/1, &set_pos_in_curr_view);
	assert_int_equal(2, lwin.list_pos);
	goto_search_match(&lwin, /*backward=*/1, /*count=*/1, &set_pos_in_curr_view);
	assert_int_equal(1, lwin.list_pos);
	goto_search_match(&lwin, /*backward=*/1, /*count=*/1, &set_pos_in_curr_view);
	assert_int_equal(2, lwin.list_pos);
}

TEST(match_navigation_without_wrapping)
{
	cfg.wrap_scan = 0;

	search_pattern(&lwin, "dos", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_int_equal(2, lwin.matches);

	lwin.list_pos = 0;

	/* Forward. */
	goto_search_match(&lwin, /*backward=*/0, /*count=*/1, &set_pos_in_curr_view);
	assert_int_equal(1, lwin.list_pos);
	goto_search_match(&lwin, /*backward=*/0, /*count=*/1, &set_pos_in_curr_view);
	assert_int_equal(2, lwin.list_pos);
	goto_search_match(&lwin, /*backward=*/0, /*count=*/1, &set_pos_in_curr_view);
	assert_int_equal(2, lwin.list_pos);

	/* Backward. */
	goto_search_match(&lwin, /*backward=*/1, /*count=*/1, &set_pos_in_curr_view);
	assert_int_equal(1, lwin.list_pos);
	goto_search_match(&lwin, /*backward=*/1, /*count=*/1, &set_pos_in_curr_view);
	assert_int_equal(1, lwin.list_pos);
}

TEST(view_patterns_are_synchronized)
{
	strcpy(rwin.curr_dir, lwin.curr_dir);
	populate_dir_list(&rwin, 0);

	search_pattern(&rwin, "do", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_int_equal(2, rwin.matches);

	search_pattern(&lwin, "dos", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_int_equal(2, lwin.matches);
	/* Different patterns cause reset. */
	assert_int_equal(0, rwin.matches);

	search_pattern(&rwin, "dos", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_int_equal(2, rwin.matches);
	/* Same patterns don't cause reset. */
	assert_int_equal(2, lwin.matches);
}

TEST(modnorm_find_returns_zero_if_msg_is_not_printed)
{
	int found;

	cfg.hl_search = 1;
	cfg.wrap_scan = 0;

	lwin.list_pos = 2;
	modnorm_set_search_attrs(/*count=*/1, /*last_search_backward=*/0);
	assert_int_equal(0, modnorm_find(&lwin, "dos", /*backward=*/0,
				/*print_msg=*/1, &found));
	assert_int_equal(2, lwin.list_pos);
	assert_false(found);

	cfg.hl_search = 0;
}

TEST(matching_directories)
{
	cfg.hl_search = 1;

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(chdir(TEST_DATA_PATH "/tree"));
	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));
	populate_dir_list(&lwin, 0);

	search_pattern(&lwin, "1/", /*stash_selection=*/cfg.hl_search,
			/*select_matches=*/cfg.hl_search);
	assert_string_equal("dir1", lwin.dir_entry[0].name);
	assert_true(lwin.dir_entry[0].selected);
	assert_string_equal("dir5", lwin.dir_entry[1].name);
	assert_false(lwin.dir_entry[1].selected);

	cfg.hl_search = 0;
}

static void
set_pos_in_curr_view(int pos)
{
	curr_view->list_pos = pos;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
