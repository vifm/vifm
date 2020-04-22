#include <stic.h>

#include <test-utils.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/status.h"

SETUP()
{
	curr_view = &lwin;

	init_modes();
	opt_handlers_setup();
}

TEARDOWN()
{
	opt_handlers_teardown();
	vle_keys_reset();
}

TEST(view_is_split_vertically_if_only_mode_was_active)
{
	curr_view = &rwin;
	other_view = &lwin;
	curr_stats.number_of_windows = 1;
	(void)vle_keys_exec_timed_out(WK_C_w WK_H);
	assert_int_equal(2, curr_stats.number_of_windows);
	assert_int_equal(VSPLIT, curr_stats.split);
}

TEST(view_is_split_horizontally_if_only_mode_was_active)
{
	curr_view = &rwin;
	other_view = &lwin;
	curr_stats.number_of_windows = 1;
	(void)vle_keys_exec_timed_out(WK_C_w WK_J);
	assert_int_equal(2, curr_stats.number_of_windows);
	assert_int_equal(HSPLIT, curr_stats.split);
}

TEST(ctrl_w_H)
{
	curr_view = &rwin;
	other_view = &lwin;
	(void)vle_keys_exec_timed_out(WK_C_w WK_H);
	assert_true(curr_view == &lwin);
	assert_true(other_view == &rwin);
}

TEST(ctrl_w_L)
{
	curr_view = &lwin;
	other_view = &rwin;
	(void)vle_keys_exec_timed_out(WK_C_w WK_L);
	assert_true(curr_view == &rwin);
	assert_true(other_view == &lwin);
}

TEST(ctrl_w_J)
{
	curr_view = &lwin;
	other_view = &rwin;
	(void)vle_keys_exec_timed_out(WK_C_w WK_J);
	assert_true(curr_view == &rwin);
	assert_true(other_view == &lwin);
}

TEST(ctrl_w_K)
{
	curr_view = &rwin;
	other_view = &lwin;
	(void)vle_keys_exec_timed_out(WK_C_w WK_K);
	assert_true(curr_view == &lwin);
	assert_true(other_view == &rwin);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
