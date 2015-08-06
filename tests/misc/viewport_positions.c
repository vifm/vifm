#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/fileview.h"

static void ensure_all_visible(int odd);

static FileView *const view = &lwin;

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

	view->list_rows = 10;
	view->column_count = 1;
	/* window_rows actually contains "number or rows - 1". */
	view->window_rows = 8 - 1;
	view->window_cells = view->window_rows + 1;
}

TEST(top_pos_at_top)
{
	view->top_line = 0;
	assert_int_equal(0, get_window_top_pos(view));
}

TEST(middle_pos_at_top)
{
	view->top_line = 0;
	assert_int_equal(3, get_window_middle_pos(view));
}

TEST(bottom_pos_at_top)
{
	view->top_line = 0;
	assert_int_equal(5, get_window_bottom_pos(view));
}

TEST(top_pos_at_middle)
{
	view->top_line = 1;
	assert_int_equal(3, get_window_top_pos(view));
}

TEST(middle_pos_at_middle)
{
	view->top_line = 1;
	assert_int_equal(4, get_window_middle_pos(view));
}

TEST(bottom_pos_at_middle)
{
	view->top_line = 1;
	assert_int_equal(6, get_window_bottom_pos(view));
}

TEST(top_pos_at_bottom)
{
	view->top_line = 2;
	assert_int_equal(4, get_window_top_pos(view));
}

TEST(middle_pos_at_bottom)
{
	view->top_line = 2;
	assert_int_equal(5, get_window_middle_pos(view));
}

TEST(bottom_pos_at_bottom)
{
	view->top_line = 2;
	assert_int_equal(9, get_window_bottom_pos(view));
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
	assert_int_equal(0, get_window_top_pos(view));
}

TEST(middle_pos_at_all_odd)
{
	ensure_all_visible(1);
	assert_int_equal(2, get_window_middle_pos(view));
}

TEST(bottom_pos_at_all_odd)
{
	ensure_all_visible(1);
	assert_int_equal(4, get_window_bottom_pos(view));
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
	assert_int_equal(0, get_window_top_pos(view));
}

TEST(middle_pos_at_all_even)
{
	ensure_all_visible(0);
	assert_int_equal(1, get_window_middle_pos(view));
}

TEST(bottom_pos_at_all_even)
{
	ensure_all_visible(0);
	assert_int_equal(3, get_window_bottom_pos(view));
}

static void
ensure_all_visible(int odd)
{
	view->top_line = 0;
	view->list_rows = odd ? 5 : 4;
	assert_true(all_files_visible(view));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
