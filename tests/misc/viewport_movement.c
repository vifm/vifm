#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/visual.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"

#include "utils.h"

static view_t *const view = &lwin;

SETUP()
{
	init_modes();

	view_setup(view);
	view->window_rows = 8;
}

TEARDOWN()
{
	view_teardown(view);

	vle_keys_reset();
}

/* 0 row----file0----file1-----
 * 1 row  | file2  | file3  |
 * 2 row  | file4  | file5  |
 * 3 row  | file6  | file7  |
 * 4 row  | file8  | file9  |
 * 5 row  | file10 | file11 |
 * 6 row  | file12 | file13 |
 * 7 row----file14---file15----
 *          file16
 */

TEST(hjkl_in_ls_normal)
{
	setup_grid(view, 2, 17, 1);
	view->list_pos = 3;

	(void)vle_keys_exec_timed_out(WK_h);
	assert_int_equal(2, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_j);
	assert_int_equal(4, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_l);
	assert_int_equal(5, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_k);
	assert_int_equal(3, view->list_pos);
}

TEST(hjkl_in_ls_visual)
{
	setup_grid(view, 2, 17, 1);
	view->list_pos = 3;

	enter_visual_mode(VS_NORMAL);

	(void)vle_keys_exec_timed_out(WK_h);
	assert_int_equal(2, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_j);
	assert_int_equal(4, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_l);
	assert_int_equal(5, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_k);
	assert_int_equal(3, view->list_pos);

	leave_visual_mode(0, 1, 0);
}

TEST(line_beginend_in_ls_normal)
{
	setup_grid(view, 2, 17, 1);
	view->list_pos = 3;

	(void)vle_keys_exec_timed_out(WK_ZERO);
	assert_int_equal(2, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_DOLLAR);
	assert_int_equal(3, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_CARET);
	assert_int_equal(2, view->list_pos);
}

TEST(line_beginend_in_ls_visual)
{
	setup_grid(view, 2, 17, 1);
	view->list_pos = 3;

	enter_visual_mode(VS_NORMAL);

	(void)vle_keys_exec_timed_out(WK_ZERO);
	assert_int_equal(2, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_DOLLAR);
	assert_int_equal(3, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_CARET);
	assert_int_equal(2, view->list_pos);

	leave_visual_mode(0, 1, 0);
}

/* 0 row----file0----file8-----
 * 1 row    file1  | file9  |
 * 2 row    file2  | file10 |
 * 3 row    file3  | file11 |
 * 4 row    file4  | file12 |
 * 5 row    file5  | file13 |
 * 6 row    file6  | file14 |
 * 7 row----file7----file15----
 */

TEST(hjkl_in_tls_normal)
{
	setup_transposed_grid(view, 2, 16, 1);
	view->list_pos = 3;

	(void)vle_keys_exec_timed_out(WK_h);
	assert_int_equal(3, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_j);
	assert_int_equal(4, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_l);
	assert_int_equal(12, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_k);
	assert_int_equal(11, view->list_pos);
}

TEST(hjkl_in_tls_visual)
{
	setup_transposed_grid(view, 2, 16, 1);
	view->list_pos = 3;

	enter_visual_mode(VS_NORMAL);

	(void)vle_keys_exec_timed_out(WK_h);
	assert_int_equal(3, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_j);
	assert_int_equal(4, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_l);
	assert_int_equal(12, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_k);
	assert_int_equal(11, view->list_pos);

	leave_visual_mode(0, 1, 0);
}

TEST(line_beginend_in_tls_normal)
{
	setup_transposed_grid(view, 2, 16, 1);
	view->list_pos = 11;

	(void)vle_keys_exec_timed_out(WK_ZERO);
	assert_int_equal(3, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_DOLLAR);
	assert_int_equal(11, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_CARET);
	assert_int_equal(3, view->list_pos);
}

TEST(line_beginend_in_tls_visual)
{
	setup_transposed_grid(view, 2, 16, 1);
	view->list_pos = 11;

	enter_visual_mode(VS_NORMAL);

	(void)vle_keys_exec_timed_out(WK_ZERO);
	assert_int_equal(3, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_DOLLAR);
	assert_int_equal(11, view->list_pos);

	(void)vle_keys_exec_timed_out(WK_CARET);
	assert_int_equal(3, view->list_pos);

	leave_visual_mode(0, 1, 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
