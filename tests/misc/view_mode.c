#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/lua/vlua.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/view.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/quickview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/matchers.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "../../src/filelist.h"
#include "../../src/filetype.h"
#include "../../src/running.h"
#include "../../src/status.h"

static int start_view_mode(const char pattern[], const char viewers[],
		const char base_dir[], const char sub_path[]);

SETUP_ONCE()
{
	curr_stats.preview.on = 0;
}

SETUP()
{
	view_setup(&lwin);
	lwin.window_rows = 1;
	view_setup(&rwin);
	rwin.window_rows = 1;
	rwin.window_cols = 1;

	curr_view = &lwin;
	other_view = &rwin;

	init_modes();

	conf_setup();
}

TEARDOWN()
{
	if(vle_mode_is(VIEW_MODE))
	{
		modview_leave();
	}

	conf_teardown();

	view_teardown(&lwin);
	view_teardown(&rwin);
	curr_view = NULL;
	other_view = NULL;

	vle_keys_reset();
	vle_cmds_reset();
	ft_reset(0);
}

TEST(initialization, IF(not_windows))
{
	/* With empty output. */
	assert_true(start_view_mode("*", "true", TEST_DATA_PATH, "read"));
	assert_string_equal("true", modview_current_viewer(lwin.vi));

	(void)vle_keys_exec_timed_out(WK_q);
	ft_reset(0);

	/* With non-empty output. */
	assert_true(start_view_mode("*", "echo 1", TEST_DATA_PATH, "read"));
	assert_string_equal("echo 1", modview_current_viewer(lwin.vi));
}

TEST(toggling_raw_mode)
{
	assert_true(start_view_mode("*", "echo 1, echo 2, echo 3", TEST_DATA_PATH,
				"read"));

	assert_false(modview_is_raw(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_i);
	assert_true(modview_is_raw(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_i);
	assert_false(modview_is_raw(lwin.vi));
}

TEST(switching_between_viewers)
{
	assert_true(start_view_mode("*", "echo 1, echo 2, echo 3", TEST_DATA_PATH,
				"read"));

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
	assert_true(start_view_mode("*[^/]", "echo 1, echo 2, echo 3", TEST_DATA_PATH,
				""));

	assert_string_equal(NULL, modview_current_viewer(lwin.vi));
}

TEST(command_for_quickview_is_not_expanded_again)
{
	curr_stats.number_of_windows = 2;
	opt_handlers_setup();
	setup_grid(curr_view, 1, 1, 1);
	update_string(&curr_view->dir_entry[0].name, "fake");

	int save_msg;
	assert_true(rn_ext(curr_view, "echo %d%c", "title", MF_PREVIEW_OUTPUT, 0,
		&save_msg) < 0);

	strlist_t lines = modview_lines(curr_stats.preview.explore);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("%d%c", lines.items[0]);

	opt_handlers_teardown();

	curr_stats.preview.on = 0;
}

TEST(quickview_command_with_input_redirection, IF(have_cat))
{
	curr_stats.number_of_windows = 2;
	opt_handlers_setup();
	setup_grid(curr_view, 1, 1, 1);
	update_string(&curr_view->dir_entry[0].name, "fake");
	curr_view->dir_entry[0].marked = 1;
	curr_view->pending_marking = 1;

	int save_msg;
	assert_true(rn_ext(curr_view, "cat", "title",
				MF_PREVIEW_OUTPUT | MF_PIPE_FILE_LIST, 0, &save_msg) < 0);

	strlist_t lines = modview_lines(curr_stats.preview.explore);
	assert_int_equal(1, lines.nitems);
	assert_string_equal("/path/fake", lines.items[0]);

	opt_handlers_teardown();

	curr_stats.preview.on = 0;
}

TEST(scrolling_in_view_mode)
{
	make_file(SANDBOX_PATH "/file", "1\n2\n3\nlast");
	assert_true(start_view_mode("*", NULL, SANDBOX_PATH, ""));

	strlist_t lines = modview_lines(lwin.vi);
	assert_int_equal(4, lines.nitems);
	assert_string_equal("1", lines.items[0]);
	assert_string_equal("2", lines.items[1]);
	assert_string_equal("3", lines.items[2]);
	assert_string_equal("last", lines.items[3]);

	(void)vle_keys_exec_timed_out(WK_j);
	assert_int_equal(1, modview_current_line(lwin.vi));
	(void)vle_keys_exec_timed_out(L"10" WK_j);
	assert_int_equal(3, modview_current_line(lwin.vi));
	(void)vle_keys_exec_timed_out(L"1" WK_k);
	assert_int_equal(2, modview_current_line(lwin.vi));

	(void)vle_keys_exec_timed_out(WK_g);
	assert_int_equal(0, modview_current_line(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_G);
	assert_int_equal(3, modview_current_line(lwin.vi));
	(void)vle_keys_exec_timed_out(L"2" WK_G);
	assert_int_equal(1, modview_current_line(lwin.vi));

	(void)vle_keys_exec_timed_out(WK_f);
	assert_int_equal(2, modview_current_line(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_b);
	assert_int_equal(1, modview_current_line(lwin.vi));

	(void)vle_keys_exec_timed_out(WK_d);
	assert_int_equal(2, modview_current_line(lwin.vi));
	(void)vle_keys_exec_timed_out(WK_u);
	assert_int_equal(1, modview_current_line(lwin.vi));

	remove_file(SANDBOX_PATH "/file");
}

TEST(searching_in_view_mode)
{
	curr_stats.save_msg = 0;

	make_file(SANDBOX_PATH "/file", "1\n2\n3\nlast");
	assert_true(start_view_mode("*", NULL, SANDBOX_PATH, ""));

	(void)vle_keys_exec_timed_out(L"/[0-9]");
	(void)vle_keys_exec_timed_out(WK_CR);
	assert_int_equal(1, modview_current_line(lwin.vi));
	assert_int_equal(0, curr_stats.save_msg);

	(void)vle_keys_exec_timed_out(WK_n);
	assert_int_equal(2, modview_current_line(lwin.vi));
	assert_int_equal(0, curr_stats.save_msg);
	(void)vle_keys_exec_timed_out(WK_n);
	assert_int_equal(2, modview_current_line(lwin.vi));
	assert_int_equal(1, curr_stats.save_msg);

	curr_stats.save_msg = 0;

	(void)vle_keys_exec_timed_out(WK_N);
	assert_int_equal(1, modview_current_line(lwin.vi));
	assert_int_equal(0, curr_stats.save_msg);
	(void)vle_keys_exec_timed_out(WK_N);
	assert_int_equal(0, modview_current_line(lwin.vi));
	assert_int_equal(0, curr_stats.save_msg);
	(void)vle_keys_exec_timed_out(WK_N);
	assert_int_equal(0, modview_current_line(lwin.vi));
	assert_int_equal(1, curr_stats.save_msg);

	remove_file(SANDBOX_PATH "/file");
}

TEST(operations_with_empty_output)
{
	assert_true(start_view_mode("*", "true", TEST_DATA_PATH, "read"));

	/* They just shouldn't crash. */
	(void)vle_keys_exec_timed_out(WK_g);
	(void)vle_keys_exec_timed_out(WK_PERCENT);
	(void)vle_keys_exec_timed_out(L"?[0-9]");
	(void)vle_keys_exec_timed_out(WK_CR);
	(void)vle_keys_exec_timed_out(WK_n);
	(void)vle_keys_exec_timed_out(WK_N);

	modview_ruler_update();
}

TEST(cmd_v)
{
	assert_true(start_view_mode(NULL, NULL, TEST_DATA_PATH, "scripts"));

	curr_stats.vlua = vlua_init();

	assert_success(vlua_run_string(curr_stats.vlua,
				"function handler(info)"
				"  local s = ginfo ~= nil"
				"  ginfo = info"
				"  return { success = s }"
				"end"));
	assert_success(vlua_run_string(curr_stats.vlua,
				"vifm.addhandler{ name = 'editor', handler = handler }"));

	curr_stats.exec_env_type = EET_EMULATOR;
	update_string(&cfg.vi_command, "#vifmtest#editor");

	int i;
	for(i = 0; i < 2; ++i)
	{
		(void)vle_keys_exec_timed_out(WK_v);

		assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.action)"));
		assert_string_equal("edit-one", ui_sb_last());
		assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.path)"));
		assert_true(ends_with(ui_sb_last(), "scripts/append-env.vifm"));
		assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.mustwait)"));
		assert_string_equal("false", ui_sb_last());
		assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.line)"));
		assert_string_equal("1", ui_sb_last());
		assert_success(vlua_run_string(curr_stats.vlua, "print(ginfo.column)"));
		assert_string_equal("nil", ui_sb_last());

		assert_success(vlua_run_string(curr_stats.vlua, "ginfo = {}"));
	}

	vlua_finish(curr_stats.vlua);
	curr_stats.vlua = NULL;
}

TEST(views_with_dirs_can_be_reattached)
{
	opt_handlers_setup();

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);
	populate_dir_list(&lwin, 0);
	/* The next call is to reset state of modview. */
	assert_false(modview_detached_draw());
	assert_success(qv_ensure_is_shown());

	/* Enter the view. */
	(void)vle_keys_exec_timed_out(WK_C_w WK_C_w);
	assert_true(vle_mode_is(VIEW_MODE));

	/* Scroll down a bit. */
	(void)vle_keys_exec_timed_out(L"2j");
	assert_int_equal(2, modview_current_line(curr_stats.preview.explore));

	/* Detach the view. */
	(void)vle_keys_exec_timed_out(WK_C_w WK_C_w);
	assert_true(vle_mode_is(NORMAL_MODE));

	/* Reattach it. */
	(void)vle_keys_exec_timed_out(WK_C_w WK_C_w);
	assert_true(vle_mode_is(VIEW_MODE));

	/* It was resurrected. */
	assert_true(modview_is_detached(curr_stats.preview.explore));
	/* Its internal state probably wasn't reset if it allows scrolling. */
	(void)vle_keys_exec_timed_out(L"k");
	assert_int_equal(1, modview_current_line(curr_stats.preview.explore));
	(void)vle_keys_exec_timed_out(L"k");
	assert_int_equal(0, modview_current_line(curr_stats.preview.explore));

	opt_handlers_teardown();
}

static int
start_view_mode(const char pattern[], const char viewers[],
		const char base_dir[], const char sub_path[])
{
	if(viewers != NULL)
	{
		char *error;
		matchers_t *ms = matchers_alloc(pattern, 0, 1, "", &error);
		assert_non_null(ms);
		ft_set_viewers(ms, viewers);
	}

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), base_dir, sub_path, NULL);
	populate_dir_list(&lwin, 0);
	(void)vle_keys_exec_timed_out(WK_e);

	return vle_mode_is(VIEW_MODE);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
