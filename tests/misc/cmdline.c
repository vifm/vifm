#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/curses.h"
#include "../../src/compat/os.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/builtin_functions.h"
#include "../../src/cmd_core.h"
#include "../../src/event_loop.h"
#include "../../src/filelist.h"
#include "../../src/flist_hist.h"
#include "../../src/status.h"

static void prompt_callback(const char response[], void *arg);
static void column_line_print(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info);

static line_stats_t *stats;

static char *prompt_response;
static int prompt_invocation_count;

SETUP_ONCE()
{
	stats = get_line_stats();
	try_enable_utf8_locale();
	init_builtin_functions();
}

SETUP()
{
	modes_init();

	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);
	view_setup(&rwin);
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	(void)vle_keys_exec_timed_out(WK_C_c);

	vle_keys_reset();
	cfg.inc_search = 0;
}

TEST(expr_reg_completion)
{
	(void)vle_keys_exec_timed_out(L":" WK_C_r WK_EQUALS);
	(void)vle_keys_exec_timed_out(L"ex" WK_C_i);
	assert_wstring_equal(L"executable(", stats->line);
	(void)vle_keys_exec_timed_out(WK_C_c);
}

TEST(expr_reg_completion_ignores_pipe)
{
	(void)vle_keys_exec_timed_out(L":" WK_C_r WK_EQUALS);
	(void)vle_keys_exec_timed_out(L"ab|ex" WK_C_i);
	assert_wstring_equal(L"ab|ex", stats->line);
	(void)vle_keys_exec_timed_out(WK_C_c);
}

TEST(prompt_cb_is_called_on_success)
{
	update_string(&prompt_response, NULL);
	prompt_invocation_count = 0;

	modcline_prompt("(prompt)", "initial", &prompt_callback, /*cb_arg=*/NULL,
			/*complete=*/NULL, /*allow_ee=*/0);
	assert_true(vle_mode_is(CMDLINE_MODE));
	assert_int_equal(CLS_PROMPT, stats->sub_mode);
	(void)vle_keys_exec_timed_out(WK_CR);

	assert_string_equal("initial", prompt_response);
	assert_int_equal(1, prompt_invocation_count);
}

TEST(prompt_cb_is_called_on_cancellation)
{
	update_string(&prompt_response, NULL);
	prompt_invocation_count = 0;

	modcline_prompt("(prompt)", "initial", &prompt_callback, /*cb_arg=*/NULL,
			/*complete=*/NULL, /*allow_ee=*/0);
	assert_true(vle_mode_is(CMDLINE_MODE));
	assert_int_equal(CLS_PROMPT, stats->sub_mode);
	(void)vle_keys_exec_timed_out(WK_C_c);

	assert_string_equal(NULL, prompt_response);
	assert_int_equal(1, prompt_invocation_count);
}

TEST(user_prompt_accepts_input)
{
	cfg.timeout_len = 1;
	ui_sb_msg("");

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(L"suffix" WK_CR);
	(void)vle_keys_exec_timed_out(L":echo input('prompt', 'input')" WK_CR);

	assert_string_equal("inputsuffix", ui_sb_last());
}

TEST(user_prompt_handles_cancellation)
{
	cfg.timeout_len = 1;
	ui_sb_msg("old");

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(L"suffix" WK_C_c);
	(void)vle_keys_exec_timed_out(L":echo input('prompt', 'input')" WK_CR);

	assert_string_equal("", ui_sb_last());
}

TEST(user_prompt_nests)
{
	cfg.timeout_len = 1;
	ui_sb_msg("");

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(L"-" WK_CR L"*" WK_CR);
	(void)vle_keys_exec_timed_out(
			L":echo input('p2', input('p1', '1').'2')" WK_CR);

	assert_string_equal("1-2*", ui_sb_last());
}

TEST(user_prompt_and_expr_reg)
{
	cfg.timeout_len = 1;
	ui_sb_msg("");

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(WK_C_r WK_EQUALS
			"input('n')" WK_CR
			L"nested" WK_CR
			L"extra" WK_CR);
	(void)vle_keys_exec_timed_out(L":echo input('p').'out'" WK_CR);

	assert_string_equal("nestedextraout", ui_sb_last());
}

TEST(user_prompt_completion)
{
	cfg.timeout_len = 1;
	ui_sb_msg("");
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", NULL);

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(WK_C_i WK_CR);
	(void)vle_keys_exec_timed_out(L":echo input('p', 'read/dos', 'dir')" WK_CR);
	assert_string_equal("read/dos", ui_sb_last());

	/* Preparing input beforehand, because input() runs nested event loop. */
	feed_keys(WK_C_i WK_CR);
	(void)vle_keys_exec_timed_out(L":echo input('p', 'read/dos', 'file')" WK_CR);
	assert_string_equal("read/dos-eof", ui_sb_last());
}

TEST(each_filtering_prompt_gets_clean_state)
{
	conf_setup();
	cfg.inc_search = 1;

	/* In order to handle input change, old input must be stored somewhere and
	 * compared against the new state.  Make sure the storage is cleared on every
	 * prompt. */

	(void)vle_keys_exec_timed_out(L"=a");
	assert_string_equal("a", curr_view->local_filter.filter.raw);
	(void)vle_keys_exec_timed_out(WK_ESC);
	assert_string_equal("", curr_view->local_filter.filter.raw);

	(void)vle_keys_exec_timed_out(L"=a");
	assert_string_equal("a", curr_view->local_filter.filter.raw);
	(void)vle_keys_exec_timed_out(WK_ESC);

	conf_teardown();
}

TEST(cmdline_navigation)
{
	/* This doesn't work outside of search and local filter submodes. */
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "tree", NULL);
	(void)vle_keys_exec_timed_out(L":");

	(void)vle_keys_exec_timed_out(WK_C_y);
	assert_false(stats->navigating);

	(void)vle_keys_exec_timed_out(WK_C_o);
	assert_string_equal("tree", get_last_path_component(curr_view->curr_dir));
}

TEST(navigation_requires_interactivity)
{
	(void)vle_keys_exec_timed_out(L"/");

	cfg.inc_search = 0;
	(void)vle_keys_exec_timed_out(WK_C_y);
	assert_false(stats->navigating);

	cfg.inc_search = 1;
	(void)vle_keys_exec_timed_out(WK_C_y);
	assert_true(stats->navigating);
	(void)vle_keys_exec_timed_out(WK_C_y);
	assert_false(stats->navigating);
}

TEST(navigation_movement)
{
	conf_setup();
	cfg.inc_search = 1;

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "read", NULL);
	populate_dir_list(curr_view, /*reload=*/0);

	(void)vle_keys_exec_timed_out(L"/");

	(void)vle_keys_exec_timed_out(WK_C_y);
	assert_string_equal("binary-data",
			curr_view->dir_entry[curr_view->list_pos].name);
	(void)vle_keys_exec_timed_out(WK_C_n);
	assert_string_equal("dos-eof",
			curr_view->dir_entry[curr_view->list_pos].name);
	(void)vle_keys_exec_timed_out(WK_C_n);
	assert_string_equal("dos-line-endings",
			curr_view->dir_entry[curr_view->list_pos].name);
	(void)vle_keys_exec_timed_out(WK_C_p);
	assert_string_equal("dos-eof",
			curr_view->dir_entry[curr_view->list_pos].name);

#ifdef ENABLE_EXTENDED_KEYS
	wchar_t keys[2] = { };

	keys[0] = K(KEY_UP);
	(void)vle_keys_exec_timed_out(keys);
	assert_string_equal("binary-data",
			curr_view->dir_entry[curr_view->list_pos].name);
	keys[0] = K(KEY_DOWN);
	(void)vle_keys_exec_timed_out(keys);
	assert_string_equal("dos-eof",
			curr_view->dir_entry[curr_view->list_pos].name);
	keys[0] = K(KEY_HOME);
	(void)vle_keys_exec_timed_out(keys);
	assert_string_equal("binary-data",
			curr_view->dir_entry[curr_view->list_pos].name);
	keys[0] = K(KEY_END);
	(void)vle_keys_exec_timed_out(keys);
	assert_string_equal("very-long-line",
			curr_view->dir_entry[curr_view->list_pos].name);
	keys[0] = K(KEY_LEFT);
	(void)vle_keys_exec_timed_out(keys);
	assert_string_equal("test-data",
			get_last_path_component(curr_view->curr_dir));
	keys[0] = K(KEY_RIGHT);
	(void)vle_keys_exec_timed_out(keys);
	assert_string_equal("read", get_last_path_component(curr_view->curr_dir));

	/* Setup for scrolling. */
	curr_view->window_rows = 5;
	setup_grid(curr_view, /*column_count=*/1, curr_view->list_rows, /*init=*/0);
	curr_view->top_line = 1;
	curr_view->list_pos = curr_view->list_rows - 1;

	keys[0] = K(KEY_PPAGE);
	(void)vle_keys_exec_timed_out(keys);
	assert_string_equal("dos-line-endings",
			curr_view->dir_entry[curr_view->list_pos].name);
	keys[0] = K(KEY_NPAGE);
	(void)vle_keys_exec_timed_out(keys);
	assert_string_equal("two-lines",
			curr_view->dir_entry[curr_view->list_pos].name);
#endif

	conf_teardown();
}

TEST(search_navigation)
{
	conf_setup();
	histories_init(5);
	cfg.inc_search = 1;
	cfg.wrap_scan = 1;

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "tree", NULL);
	populate_dir_list(curr_view, /*reload=*/0);

	(void)vle_keys_exec_timed_out(L"/" WK_C_y);

	/* Can enter and leave directories. */
	(void)vle_keys_exec_timed_out(L"5" WK_C_m);
	assert_string_equal("dir5", get_last_path_component(curr_view->curr_dir));
	(void)vle_keys_exec_timed_out(WK_C_o);
	assert_string_equal("tree", get_last_path_component(curr_view->curr_dir));
	(void)vle_keys_exec_timed_out(L"1" WK_C_m);
	assert_string_equal("dir1", get_last_path_component(curr_view->curr_dir));
	(void)vle_keys_exec_timed_out(L"2" WK_C_m);
	assert_string_equal("dir2", get_last_path_component(curr_view->curr_dir));

	assert_true(hist_is_empty(&curr_stats.search_hist));

	cfg.wrap_scan = 0;
	histories_init(0);
	conf_teardown();
}

/* Same as search_navigation test above.  Duplicating because filtering is more
 * complicated and it's a good idea to verify it also works fine. */
TEST(filter_navigation)
{
	conf_setup();
	histories_init(5);
	cfg.inc_search = 1;
	cfg.wrap_scan = 1;

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "tree", NULL);
	populate_dir_list(curr_view, /*reload=*/0);

	(void)vle_keys_exec_timed_out(L"=" WK_C_y);

	/* Can enter and leave directories. */
	(void)vle_keys_exec_timed_out(L"5" WK_C_m);
	assert_string_equal("dir5", get_last_path_component(curr_view->curr_dir));
	(void)vle_keys_exec_timed_out(WK_C_o);
	assert_string_equal("tree", get_last_path_component(curr_view->curr_dir));
	(void)vle_keys_exec_timed_out(L"1" WK_C_m);
	assert_string_equal("dir1", get_last_path_component(curr_view->curr_dir));
	(void)vle_keys_exec_timed_out(L"2" WK_C_m);
	assert_string_equal("dir2", get_last_path_component(curr_view->curr_dir));

	assert_true(hist_is_empty(&curr_stats.filter_hist));

	cfg.wrap_scan = 0;
	histories_init(0);
	conf_teardown();
}

TEST(can_leave_navigation_without_running)
{
	conf_setup();
	cfg.inc_search = 1;
	cfg.wrap_scan = 1;

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "tree", NULL);
	populate_dir_list(curr_view, /*reload=*/0);

	(void)vle_keys_exec_timed_out(L"=" WK_C_y L"h" WK_C_j);
	assert_int_equal(1, curr_view->list_rows);

	cfg.wrap_scan = 0;
	conf_teardown();
}

TEST(normal_in_autocmd_does_not_break_filter_navigation)
{
	conf_setup();
	assert_success(stats_init(&cfg));
	cfg.inc_search = 1;

	assert_success(cmds_dispatch1("autocmd DirEnter * normal ga", curr_view,
				CIT_COMMAND));

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "tree", NULL);
	populate_dir_list(curr_view, /*reload=*/0);

	(void)vle_keys_exec_timed_out(L"=" WK_C_y WK_C_m);
	assert_string_equal("", curr_view->local_filter.filter.raw);

	assert_success(cmds_dispatch1("autocmd!", curr_view, CIT_COMMAND));
	wait_for_bg();

	conf_teardown();
}

TEST(filter_in_autocmd_does_not_break_filter_navigation)
{
	conf_setup();
	histories_init(5);
	cfg.inc_search = 1;
	cfg.auto_ch_pos = 1;
	cfg.ch_pos_on = CHPOS_ENTER;

	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);
	columns_set_line_print_func(&column_line_print);
	curr_view->columns = columns_create();
	opt_handlers_setup();

	assert_success(cmds_dispatch1("autocmd DirEnter tree/ filter! dir", curr_view,
				CIT_COMMAND));

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "tree", NULL);
	populate_dir_list(curr_view, /*reload=*/0);
	assert_int_equal(3, curr_view->list_rows);
	curr_view->list_pos = 2;
	flist_hist_save(curr_view);

	curr_stats.load_stage = 2;
	(void)vle_keys_exec_timed_out(L"h/" WK_C_y WK_C_m);
	/* This shouldn't lead to assertion/segfault caused by cursor position being
	 * outside of file list. */
	(void)vle_keys_exec_timed_out(L"d");
	curr_stats.load_stage = 0;
	assert_int_equal(1, curr_view->list_rows);
	/* Position of the filtered list is the one being stored. */
	assert_int_equal(0, stats->old_top);
	assert_int_equal(0, stats->old_pos);

	(void)vle_keys_exec_timed_out(WK_C_c);
	assert_success(cmds_dispatch1("autocmd!", curr_view, CIT_COMMAND));

	opt_handlers_teardown();
	columns_teardown();

	histories_init(0);
	conf_teardown();
	cfg.auto_ch_pos = 0;
	cfg.ch_pos_on = 0;
}

TEST(filtering_does_not_result_in_invalid_position)
{
	conf_setup();
	histories_init(5);
	cfg.inc_search = 1;

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "tree", NULL);
	populate_dir_list(curr_view, /*reload=*/0);
	assert_int_equal(3, curr_view->list_rows);
	flist_hist_save(curr_view);

	(void)vle_keys_exec_timed_out(L"=5" WK_C_m);
	assert_int_equal(1, curr_view->list_rows);
	(void)vle_keys_exec_timed_out(L"=" WK_C_u WK_C_y);
	assert_int_equal(3, curr_view->list_rows);
	/* This shouldn't cause a crash because of incorrect cursor position (outside
	 * of file list). */
	(void)vle_keys_exec_timed_out(WK_C_o);

	(void)vle_keys_exec_timed_out(WK_C_c);

	opt_handlers_teardown();
	columns_teardown();

	histories_init(0);
	conf_teardown();
}

TEST(leaving_navigation_does_not_move_cursor)
{
	conf_setup();
	cfg.inc_search = 1;

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "existing-files", NULL);
	populate_dir_list(curr_view, /*reload=*/0);

	/* Search. */
	curr_view->list_pos = 0;
	(void)vle_keys_exec_timed_out(L"/");
	/* Empty input. */
	(void)vle_keys_exec_timed_out(WK_C_y);
	assert_int_equal(0, curr_view->list_pos);
	(void)vle_keys_exec_timed_out(WK_C_n);
	assert_int_equal(1, curr_view->list_pos);
	(void)vle_keys_exec_timed_out(WK_C_y);
	assert_int_equal(1, curr_view->list_pos);
	/* This triggers view cursor update. */
	(void)vle_keys_exec_timed_out(L"a" WK_C_h);
	assert_int_equal(1, curr_view->list_pos);
	/* Non-empty input. */
	(void)vle_keys_exec_timed_out(WK_C_y L"a");
	assert_int_equal(1, curr_view->list_pos);
	(void)vle_keys_exec_timed_out(WK_C_n);
	assert_int_equal(2, curr_view->list_pos);
	(void)vle_keys_exec_timed_out(WK_C_y);
	assert_int_equal(2, curr_view->list_pos);
	/* This triggers view cursor update. */
	(void)vle_keys_exec_timed_out(L"a" WK_C_h);
	assert_int_equal(2, curr_view->list_pos);
	(void)vle_keys_exec_timed_out(WK_C_c);

	/* Filter. */
	curr_view->list_pos = 0;
	(void)vle_keys_exec_timed_out(L"=");
	/* Empty input. */
	(void)vle_keys_exec_timed_out(WK_C_y);
	assert_int_equal(0, curr_view->list_pos);
	(void)vle_keys_exec_timed_out(WK_C_n);
	assert_int_equal(1, curr_view->list_pos);
	(void)vle_keys_exec_timed_out(WK_C_y);
	assert_int_equal(1, curr_view->list_pos);
	/* This triggers view cursor update. */
	(void)vle_keys_exec_timed_out(L"a" WK_C_h);
	assert_int_equal(1, curr_view->list_pos);
	/* Non-empty input. */
	(void)vle_keys_exec_timed_out(WK_C_y L".");
	assert_int_equal(1, curr_view->list_pos);
	(void)vle_keys_exec_timed_out(WK_C_n);
	assert_int_equal(2, curr_view->list_pos);
	(void)vle_keys_exec_timed_out(WK_C_y);
	assert_int_equal(2, curr_view->list_pos);
	/* This triggers view cursor update. */
	(void)vle_keys_exec_timed_out(L"a" WK_C_h);
	assert_int_equal(2, curr_view->list_pos);
	(void)vle_keys_exec_timed_out(WK_C_c);

	conf_teardown();
}

TEST(navigation_preserves_input_on_enter_failure, IF(regular_unix_user))
{
	conf_setup();
	create_dir(SANDBOX_PATH "/dir");
	assert_success(os_chmod(SANDBOX_PATH "/dir", 0000));

	cfg.inc_search = 1;
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir), SANDBOX_PATH,
			"", NULL);
	populate_dir_list(curr_view, /*reload=*/0);

	(void)vle_keys_exec_timed_out(L"/" WK_C_y L"di" WK_C_m);
	assert_wstring_equal(L"di", stats->line);
	(void)vle_keys_exec_timed_out(WK_C_c);

	remove_dir(SANDBOX_PATH "/dir");
	conf_teardown();
}

TEST(navigation_opens_files, IF(not_windows))
{
	conf_setup();
	cfg.inc_search = 1;
	cfg.nav_open_files = 1;
	stats_init(&cfg);

	create_executable(SANDBOX_PATH "/script");
	make_file(SANDBOX_PATH "/script",
			"#!/bin/sh\n"
			"touch " SANDBOX_PATH "/out");
	create_file(SANDBOX_PATH "/in");

	char vi_cmd[PATH_MAX + 1];
	make_abs_path(vi_cmd, sizeof(vi_cmd), SANDBOX_PATH, "script", NULL);
	update_string(&cfg.vi_command, vi_cmd);

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir), SANDBOX_PATH,
			"", NULL);
	populate_dir_list(curr_view, /*reload=*/0);

	/* This should create "out" file. */
	(void)vle_keys_exec_timed_out(L"/" WK_C_y WK_C_m);

	update_string(&cfg.vi_command, NULL);

	remove_file(SANDBOX_PATH "/in");
	remove_file(SANDBOX_PATH "/out");
	remove_file(SANDBOX_PATH "/script");

	conf_teardown();
	cfg.nav_open_files = 0;
}

static void
prompt_callback(const char response[], void *arg)
{
	update_string(&prompt_response, response);
	++prompt_invocation_count;
}

static void
column_line_print(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
	/* Do nothing. */
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
