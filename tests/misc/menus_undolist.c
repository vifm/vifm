#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/menus/undolist_menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/status.h"
#include "../../src/undo.h"

static OpsResult exec_func(OPS op, void *data, const char src[],
		const char dst[]);
static int op_avail(OPS op);

SETUP()
{
	static int undo_levels = 10;
	un_init(&exec_func, &op_avail, NULL, &undo_levels);

	init_modes();

	curr_stats.load_stage = -1;
}

TEARDOWN()
{
	un_reset();

	vle_keys_reset();

	curr_stats.load_stage = 0;
}

TEST(empty_undolist_menu_is_not_displayed)
{
	ui_sb_msg("");
	assert_failure(show_undolist_menu(&lwin, 0));
	assert_string_equal("Undolist is empty", ui_sb_last());

	ui_sb_msg("");
	assert_failure(show_undolist_menu(&lwin, 1));
	assert_string_equal("Undolist is empty", ui_sb_last());
}

TEST(undolist_allows_resetting_current_position)
{
	un_group_open("msg1");
	assert_success(un_group_add_op(OP_MOVE, NULL, NULL, "do_msg1", "undo_msg1"));
	un_group_close();

	ui_sb_msg("");
	assert_success(show_undolist_menu(&lwin, 0));
	assert_string_equal("", ui_sb_last());

	/* Pass key that's unhandled by both undolist menu and menu mode. */
	(void)vle_keys_exec_timed_out(WK_R);

	(void)vle_keys_exec_timed_out(WK_j);
	(void)vle_keys_exec_timed_out(WK_r);
	assert_int_equal(UN_ERR_NONE, un_group_undo());
	(void)vle_keys_exec_timed_out(WK_k);
	(void)vle_keys_exec_timed_out(WK_r);
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	(void)vle_keys_exec_timed_out(WK_q);
}

static OpsResult
exec_func(OPS op, void *data, const char src[], const char dst[])
{
	return OPS_SUCCEEDED;
}

static int
op_avail(OPS op)
{
	return (op == OP_MOVE);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
