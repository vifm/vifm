#include <stic.h>

#include <unistd.h> /* chdir() */

#include <string.h> /* memset() strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/modes/normal.h"
#include "../../src/utils/fs.h"
#include "../../src/filelist.h"
#include "../../src/search.h"

static char *saved_cwd;

SETUP()
{
	saved_cwd = save_cwd();

	view_setup(&lwin);
	view_setup(&rwin);

	assert_success(chdir(TEST_DATA_PATH "/read"));
	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));

	lwin.sort[0] = SK_BY_NAME;
	memset(&lwin.sort[1], SK_NONE, sizeof(lwin.sort) - 1);

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
	int found;

	cfg.hl_search = 1;

	find_pattern(&lwin, "dos", 0, 0, &found, 0);
	assert_true(found);
	assert_int_equal(2, lwin.matches);
	assert_string_equal("dos-eof", lwin.dir_entry[1].name);
	assert_true(lwin.dir_entry[1].selected);
	assert_string_equal("dos-line-endings", lwin.dir_entry[2].name);
	assert_true(lwin.dir_entry[2].selected);

	cfg.hl_search = 0;
}

TEST(nothing_happens_for_empty_pattern)
{
	int found;

	find_pattern(&lwin, "", 0, 0, &found, 0);
	assert_true(found);
	assert_int_equal(0, lwin.matches);
}

TEST(wrong_pattern_results_in_no_matches)
{
	int found;

	find_pattern(&lwin, "asdfasdfasdf", 0, 0, &found, 0);
	assert_false(found);
	assert_int_equal(0, lwin.matches);

	find_pattern(&lwin, "*", 0, 0, &found, 0);
	assert_false(found);
	assert_int_equal(0, lwin.matches);
}

TEST(search_finds_matching_files_and_numbers_them)
{
	int found;

	find_pattern(&lwin, "dos", 0, 0, &found, 0);
	assert_true(found);
	assert_int_equal(2, lwin.matches);
	assert_string_equal("dos-eof", lwin.dir_entry[1].name);
	assert_int_equal(1, lwin.dir_entry[1].search_match);
	assert_string_equal("dos-line-endings", lwin.dir_entry[2].name);
	assert_int_equal(2, lwin.dir_entry[2].search_match);
}

TEST(cursor_can_be_positioned_on_first_match)
{
	int found;

	lwin.list_pos = 0;
	find_pattern(&lwin, "dos", 0, 1, &found, 0);
	assert_string_equal("dos-eof", lwin.dir_entry[1].name);
	lwin.list_pos = 1;
}

TEST(reset_clears_counter_and_match_numbers)
{
	int found;
	int i;

	find_pattern(&lwin, "dos", 0, 0, &found, 0);
	assert_true(found);
	assert_int_equal(2, lwin.matches);

	reset_search_results(&lwin);

	assert_int_equal(0, lwin.matches);

	for(i = 0; i < lwin.list_rows; ++i)
	{
		assert_int_equal(0, lwin.dir_entry[i].search_match);
	}
}

TEST(match_navigation_with_wrapping)
{
	int found;

	cfg.wrap_scan = 1;

	find_pattern(&lwin, "dos", 0, 0, &found, 0);
	assert_true(found);
	assert_int_equal(2, lwin.matches);

	lwin.list_pos = 0;

	/* Forward. */
	goto_search_match(&lwin, 0);
	assert_int_equal(1, lwin.list_pos);
	goto_search_match(&lwin, 0);
	assert_int_equal(2, lwin.list_pos);
	goto_search_match(&lwin, 0);
	assert_int_equal(1, lwin.list_pos);

	/* Backward. */
	goto_search_match(&lwin, 1);
	assert_int_equal(2, lwin.list_pos);
	goto_search_match(&lwin, 1);
	assert_int_equal(1, lwin.list_pos);
	goto_search_match(&lwin, 1);
	assert_int_equal(2, lwin.list_pos);
}

TEST(match_navigation_without_wrapping)
{
	int found;

	cfg.wrap_scan = 0;

	find_pattern(&lwin, "dos", 0, 0, &found, 0);
	assert_true(found);
	assert_int_equal(2, lwin.matches);

	lwin.list_pos = 0;

	/* Forward. */
	goto_search_match(&lwin, 0);
	assert_int_equal(1, lwin.list_pos);
	goto_search_match(&lwin, 0);
	assert_int_equal(2, lwin.list_pos);
	goto_search_match(&lwin, 0);
	assert_int_equal(2, lwin.list_pos);

	/* Backward. */
	goto_search_match(&lwin, 1);
	assert_int_equal(1, lwin.list_pos);
	goto_search_match(&lwin, 1);
	assert_int_equal(1, lwin.list_pos);
}

TEST(view_patterns_are_synchronized)
{
	int found;

	strcpy(rwin.curr_dir, lwin.curr_dir);
	populate_dir_list(&rwin, 0);

	find_pattern(&rwin, "do", 0, 0, &found, 0);
	assert_int_equal(2, rwin.matches);

	find_pattern(&lwin, "dos", 0, 0, &found, 0);
	assert_int_equal(2, lwin.matches);
	/* Different patterns cause reset. */
	assert_int_equal(0, rwin.matches);

	find_pattern(&rwin, "dos", 0, 0, &found, 0);
	assert_int_equal(2, rwin.matches);
	/* Same patterns don't cause reset. */
	assert_int_equal(2, lwin.matches);
}

TEST(find_npattern_returns_zero_if_msg_is_not_printed)
{
	cfg.hl_search = 1;
	cfg.inc_search = 0;

	assert_int_equal(0, modnorm_find(&lwin, "dos", 0, 1));

	cfg.hl_search = 0;
}

TEST(matching_directories)
{
	int found;

	cfg.hl_search = 1;

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();

	assert_success(chdir(TEST_DATA_PATH "/tree"));
	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));
	populate_dir_list(&lwin, 0);

	find_pattern(&lwin, "1/", 0, 0, &found, 0);
	assert_true(found);
	assert_string_equal("dir1", lwin.dir_entry[0].name);
	assert_true(lwin.dir_entry[0].selected);
	assert_string_equal("dir5", lwin.dir_entry[1].name);
	assert_false(lwin.dir_entry[1].selected);

	cfg.hl_search = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
