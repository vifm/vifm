#include <stic.h>

#include <test-utils.h>

#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/view.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/matchers.h"
#include "../../src/filelist.h"
#include "../../src/filetype.h"
#include "../../src/status.h"

SETUP_ONCE()
{
	curr_stats.preview.on = 0;
}

SETUP()
{
	view_setup(&lwin);
	curr_view = &lwin;
	other_view = &rwin;

	init_modes();

	conf_setup();

	char *error;
	matchers_t *ms = matchers_alloc("*", 0, 1, "", &error);
	assert_non_null(ms);
	ft_set_viewers(ms, "echo 1, echo 2, echo 3");

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "read",
			NULL);
	populate_dir_list(&lwin, 0);
	(void)vle_keys_exec_timed_out(WK_e);
	assert_true(vle_mode_is(VIEW_MODE));
}

TEARDOWN()
{
	(void)vle_keys_exec_timed_out(WK_q);

	conf_teardown();

	view_teardown(&lwin);
	curr_view = NULL;
	other_view = NULL;

	vle_keys_reset();
	vle_cmds_reset();
	ft_reset(0);
}

TEST(toggling_raw_mode)
{
	assert_false(modview_is_raw(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_i);
	assert_true(modview_is_raw(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_i);
	assert_false(modview_is_raw(lwin.vi));
}

TEST(switching_between_viewers)
{
	assert_string_equal("echo 1", modview_current_viewer(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_a);
	assert_string_equal("echo 2", modview_current_viewer(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_a);
	assert_string_equal("echo 3", modview_current_viewer(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_a);
	assert_string_equal("echo 1", modview_current_viewer(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_A);
	assert_string_equal("echo 3", modview_current_viewer(lwin.vi));

	(void)vle_keys_exec_timed_out(WK_i);
	assert_true(modview_is_raw(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_a);
	assert_string_equal("echo 3", modview_current_viewer(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_A);
	assert_string_equal("echo 3", modview_current_viewer(lwin.vi));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
