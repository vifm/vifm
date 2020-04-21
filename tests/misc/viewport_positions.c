#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/flist_pos.h"

static void ensure_all_visible(int odd);

static view_t *const view = &lwin;

/*          file0
 * 0 row----file1---- <= top
 * 1 row  | file2 |
 * 2 row  | file3 |   <= top + offset
 * 3 row  | file4 |   <= middle
 * 4 row  | file5 |
 * 5 row  | file6 |   <= bottom - offset
 * 6 row  | file7 |
 * 7 row----file8---- <= bottom
 *          file9
 */

SETUP()
{
	cfg.scroll_off = 2;

	view->window_rows = 8;

	setup_grid(view, 1, 10, 0);
}

TEST(top_pos_at_top)
{
	view->top_line = 0;
	assert_int_equal(0, fpos_get_top_pos(view));
}

TEST(middle_pos_at_top)
{
	view->top_line = 0;
	assert_int_equal(3, fpos_get_middle_pos(view));
}

TEST(bottom_pos_at_top)
{
	view->top_line = 0;
	assert_int_equal(5, fpos_get_bottom_pos(view));
}

TEST(top_pos_at_middle)
{
	view->top_line = 1;
	assert_int_equal(3, fpos_get_top_pos(view));
}

TEST(middle_pos_at_middle)
{
	view->top_line = 1;
	assert_int_equal(4, fpos_get_middle_pos(view));
}

TEST(bottom_pos_at_middle)
{
	view->top_line = 1;
	assert_int_equal(6, fpos_get_bottom_pos(view));
}

TEST(top_pos_at_bottom)
{
	view->top_line = 2;
	assert_int_equal(4, fpos_get_top_pos(view));
}

TEST(middle_pos_at_bottom)
{
	view->top_line = 2;
	assert_int_equal(5, fpos_get_middle_pos(view));
}

TEST(bottom_pos_at_bottom)
{
	view->top_line = 2;
	assert_int_equal(9, fpos_get_bottom_pos(view));
}

/* 0 row----file0---- <= top [ + offset ]
 * 1 row  | file1 |
 * 2 row  | file2 |   <= middle
 * 3 row  | file3 |
 * 4 row  | file4 |   <= bottom [ - offset ]
 * 5 row  |       |
 * 6 row  |       |
 * 7 row-------------
 */

TEST(top_pos_at_all_odd)
{
	ensure_all_visible(1);
	assert_int_equal(0, fpos_get_top_pos(view));
}

TEST(middle_pos_at_all_odd)
{
	ensure_all_visible(1);
	assert_int_equal(2, fpos_get_middle_pos(view));
}

TEST(bottom_pos_at_all_odd)
{
	ensure_all_visible(1);
	assert_int_equal(4, fpos_get_bottom_pos(view));
}

/* 0 row----file0---- <= top [ + offset ]
 * 1 row  | file1 |   <= middle
 * 2 row  | file2 |
 * 3 row  | file3 |   <= bottom [ - offset ]
 * 4 row  |       |
 * 5 row  |       |
 * 6 row  |       |
 * 7 row-------------
 */

TEST(top_pos_at_all_even)
{
	ensure_all_visible(0);
	assert_int_equal(0, fpos_get_top_pos(view));
}

TEST(middle_pos_at_all_even)
{
	ensure_all_visible(0);
	assert_int_equal(1, fpos_get_middle_pos(view));
}

TEST(bottom_pos_at_all_even)
{
	ensure_all_visible(0);
	assert_int_equal(3, fpos_get_bottom_pos(view));
}

/* 0 row----file0----file1----- <= top
 * 1 row  | file2  | file3  |
 * 2 row  | file4  | file5  |
 * 3 row  | file6  | file7  |   <= middle
 * 4 row  | file8  | file9  |
 * 5 row  | file10 | file11 |   <= bottom - offset
 * 6 row  | file12 | file13 |
 * 7 row----file14---file15----
 *          file16
 */

TEST(top_accounts_for_columns)
{
	view->top_line = 0;
	setup_grid(view, 2, 17, 0);

	view->list_pos = 6;
	assert_int_equal(0, fpos_get_top_pos(view));

	view->list_pos = 3;
	assert_int_equal(1, fpos_get_top_pos(view));
}

TEST(middle_accounts_for_columns)
{
	view->top_line = 0;
	setup_grid(view, 2, 17, 0);

	view->list_pos = 2;
	assert_int_equal(6, fpos_get_middle_pos(view));

	view->list_pos = 11;
	assert_int_equal(7, fpos_get_middle_pos(view));
}

TEST(bottom_with_offset_accounts_for_columns)
{
	view->top_line = 0;
	setup_grid(view, 2, 17, 0);

	view->list_pos = 8;
	assert_int_equal(10, fpos_get_bottom_pos(view));

	view->list_pos = 11;
	assert_int_equal(11, fpos_get_bottom_pos(view));
}

/*          file0    file1
 * 0 row----file2----file3-----
 * 1 row  | file4  | file5  |
 * 2 row  | file6  | file7  |   <= top + offset
 * 3 row  | file8  | file9  |   <= middle
 * 4 row  | file10 | file11 |
 * 5 row  | file12 | file13 |
 * 6 row  | file14 | file15 |
 * 7 row----file16------------- <= bottom
 */

TEST(top_with_offset_accounts_for_columns)
{
	view->top_line = 2;
	setup_grid(view, 2, 17, 0);

	view->list_pos = 14;
	assert_int_equal(6, fpos_get_top_pos(view));

	view->list_pos = 3;
	assert_int_equal(7, fpos_get_top_pos(view));
}

TEST(middle_with_offset_accounts_for_columns)
{
	view->top_line = 2;
	setup_grid(view, 2, 17, 0);

	view->list_pos = 14;
	assert_int_equal(8, fpos_get_middle_pos(view));

	view->list_pos = 3;
	assert_int_equal(9, fpos_get_middle_pos(view));
}

TEST(bottom_accounts_for_columns)
{
	view->top_line = 2;
	setup_grid(view, 2, 17, 0);

	view->list_pos = 8;
	assert_int_equal(16, fpos_get_bottom_pos(view));

	view->list_pos = 5;
	assert_int_equal(15, fpos_get_bottom_pos(view));
}

/*          file0    file1
 * 0 row----file2----file3-----
 * 1 row  | file4  | file5  |
 * 2 row  | file6  | file7  |   <= top + offset
 * 3 row  | file8  | file9  |   <= middle for column 1
 * 4 row  | file10 | file11 |   <= middle for column 2
 * 5 row  | file12 | file13 |
 * 6 row----file14------------- <= bottom
 */

TEST(middle_is_specific_to_a_column)
{
	view->top_line = 2;
	setup_grid(view, 2, 15, 0);

	view->list_pos = 12;
	assert_int_equal(8, fpos_get_middle_pos(view));

	view->list_pos = 7;
	assert_int_equal(7, fpos_get_middle_pos(view));
}

/* 0 row----file0----file8----- <= top
 * 1 row  | file1  | file9  |
 * 2 row  | file2  | file10 |   <= middle for column 2
 * 3 row  | file3  | file11 |   <= middle for column 1
 * 4 row  | file4  | file12 |
 * 5 row  | file5  | file13 |   <= bottom for column 1
 * 6 row  | file6  |        |
 * 7 row----file7-------------- <= bottom for column 2
 */

TEST(top_with_accounts_for_transposed_columns)
{
	view->top_line = 0;
	setup_transposed_grid(view, 2, 14, 0);

	view->list_pos = 10;
	assert_int_equal(8, fpos_get_top_pos(view));

	view->list_pos = 1;
	assert_int_equal(0, fpos_get_top_pos(view));
}

TEST(middle_is_specific_to_a_transposed_column)
{
	view->top_line = 0;
	setup_transposed_grid(view, 2, 14, 0);

	view->list_pos = 1;
	assert_int_equal(3, fpos_get_middle_pos(view));

	view->list_pos = 12;
	assert_int_equal(10, fpos_get_middle_pos(view));
}

TEST(bottom_accounts_for_transposed_columns)
{
	view->top_line = 0;
	setup_transposed_grid(view, 2, 14, 0);

	view->list_pos = 8;
	assert_int_equal(13, fpos_get_bottom_pos(view));

	view->list_pos = 5;
	assert_int_equal(7, fpos_get_bottom_pos(view));
}

static void
ensure_all_visible(int odd)
{
	view->top_line = 0;
	view->list_rows = odd ? 5 : 4;
	assert_true(fpos_are_all_files_visible(view));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
