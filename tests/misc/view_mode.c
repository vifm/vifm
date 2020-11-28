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
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/filelist.h"
#include "../../src/filetype.h"
#include "../../src/running.h"
#include "../../src/status.h"

static void start_view_mode(const char pattern[], const char tests_dir[]);

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
	start_view_mode("*", "read");

	assert_false(modview_is_raw(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_i);
	assert_true(modview_is_raw(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_i);
	assert_false(modview_is_raw(lwin.vi));
}

TEST(switching_between_viewers)
{
	start_view_mode("*", "read");

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

TEST(directories_are_matched_separately)
{
	start_view_mode("*[^/]", "");

	assert_string_equal(NULL, modview_current_viewer(lwin.vi));
}

TEST(command_for_quickview_is_not_expanded_again)
{
	curr_stats.number_of_windows = 2;
	opt_handlers_setup();
	setup_grid(curr_view, 1, 1, 1);
	update_string(&curr_view->dir_entry[0].name, "fake");

	int save_msg;
	assert_true(rn_ext("echo %d%c", "title", MF_PREVIEW_OUTPUT, 0,
		&save_msg) < 0);

	strlist_t lines = modview_lines(curr_stats.preview.explore);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("%d%c", lines.items[0]);

	opt_handlers_teardown();
}

static void
start_view_mode(const char pattern[], const char tests_dir[])
{
	char *error;
	matchers_t *ms = matchers_alloc(pattern, 0, 1, "", &error);
	assert_non_null(ms);
	ft_set_viewers(ms, "echo 1, echo 2, echo 3");

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, tests_dir,
			NULL);
	populate_dir_list(&lwin, 0);
	(void)vle_keys_exec_timed_out(WK_e);
	assert_true(vle_mode_is(VIEW_MODE));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
