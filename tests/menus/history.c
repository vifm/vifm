#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/os.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/menus/history_menu.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

static line_stats_t *stats;

SETUP_ONCE()
{
	stats = get_line_stats();
}

SETUP()
{
	modes_init();
	cmds_init();

	curr_view = &lwin;
	view_setup(&lwin);

	histories_init(/*size=*/10);

	curr_stats.load_stage = -1;
}

TEARDOWN()
{
	vle_cmds_reset();
	vle_keys_reset();

	view_teardown(&lwin);

	cfg_resize_histories(0);

	curr_stats.load_stage = 0;
}

TEST(commands_hist_split_at_bar)
{
	hists_commands_save("echo 'a' | echo 'b'");
	assert_success(show_cmdhistory_menu(&lwin));

	ui_sb_msg("");
	(void)vle_keys_exec(WK_CR);
	assert_string_equal("b", ui_sb_last());
}

TEST(fsearch_hist_does_not_split_at_bar)
{
	hists_search_save("a|b");
	assert_success(show_fsearchhistory_menu(&lwin));

	assert_string_equal("", lwin.last_search);
	(void)vle_keys_exec(WK_CR);
	assert_string_equal("a|b", lwin.last_search);
}

TEST(bsearch_hist_does_not_split_at_bar)
{
	hists_search_save("a|b");
	assert_success(show_bsearchhistory_menu(&lwin));

	assert_string_equal("", lwin.last_search);
	(void)vle_keys_exec(WK_CR);
	assert_string_equal("a|b", lwin.last_search);
}

TEST(lfilter_hist_does_not_split_at_bar)
{
	hists_filter_save("a|b");
	assert_success(show_filterhistory_menu(&lwin));

	assert_string_equal("", lwin.local_filter.filter.raw);
	(void)vle_keys_exec(WK_CR);
	assert_string_equal("a|b", lwin.local_filter.filter.raw);
}

TEST(menu_commands_hist_runs_on_enter)
{
	opt_handlers_setup();
	assert_success(os_chdir(SANDBOX_PATH));

	hists_menucmd_save("write log");
	assert_success(show_menucmdhistory_menu(&lwin));

	(void)vle_keys_exec(WK_CR);

	remove_file("log");
	opt_handlers_teardown();
}

TEST(prompt_hist_does_nothing_on_enter)
{
	hists_prompt_save("abc");
	assert_success(show_prompthistory_menu(&lwin));

	(void)vle_keys_exec(WK_CR);
}

TEST(exprreg_hist_does_nothing_on_enter)
{
	hists_exprreg_save("abc");
	assert_success(show_exprreghistory_menu(&lwin));

	(void)vle_keys_exec(WK_CR);
}

TEST(commands_hist_allows_editing)
{
	hists_commands_save("echo 'a'");
	assert_success(show_cmdhistory_menu(&lwin));

	(void)vle_keys_exec(WK_c);
	assert_true(vle_mode_is(CMDLINE_MODE));
	assert_false(stats->search_mode);
	assert_wstring_equal(L"echo 'a'", stats->line);
	(void)vle_keys_exec_timed_out(WK_ESC);
}

TEST(menu_commands_hist_allows_editing)
{
	hists_menucmd_save("write log");
	assert_success(show_menucmdhistory_menu(&lwin));

	(void)vle_keys_exec(WK_c);
	assert_true(vle_mode_is(CMDLINE_MODE));
	assert_int_equal(MENU_MODE, vle_mode_get_primary());
	assert_wstring_equal(L"write log", stats->line);

	(void)vle_keys_exec_timed_out(WK_ESC);
	assert_true(vle_mode_is(MENU_MODE));
	(void)vle_keys_exec_timed_out(WK_ESC);
}

TEST(fsearch_hist_allows_editing)
{
	hists_search_save("a|b");
	assert_success(show_fsearchhistory_menu(&lwin));

	(void)vle_keys_exec(WK_c);
	assert_true(vle_mode_is(CMDLINE_MODE));
	assert_true(stats->search_mode);
	assert_wstring_equal(L"a|b", stats->line);
	(void)vle_keys_exec_timed_out(WK_ESC);
}

TEST(bsearch_hist_allows_editing)
{
	hists_search_save("a|b");
	assert_success(show_bsearchhistory_menu(&lwin));

	(void)vle_keys_exec(WK_c);
	assert_true(vle_mode_is(CMDLINE_MODE));
	assert_true(stats->search_mode);
	assert_wstring_equal(L"a|b", stats->line);
	(void)vle_keys_exec_timed_out(WK_ESC);
}

TEST(lfilter_allows_editing)
{
	hists_filter_save("a|b");
	assert_success(show_filterhistory_menu(&lwin));

	(void)vle_keys_exec(WK_c);
	assert_true(vle_mode_is(CMDLINE_MODE));
	assert_false(stats->search_mode);
	assert_wstring_equal(L"a|b", stats->line);
	(void)vle_keys_exec_timed_out(WK_ESC);
}

TEST(prompt_hist_does_not_allow_editing)
{
	hists_prompt_save("abc");
	assert_success(show_prompthistory_menu(&lwin));

	(void)vle_keys_exec(WK_c);
	assert_true(vle_mode_is(MENU_MODE));
}

TEST(exprreg_hist_does_not_allow_editing)
{
	hists_exprreg_save("abc");
	assert_success(show_exprreghistory_menu(&lwin));

	(void)vle_keys_exec(WK_c);
	assert_true(vle_mode_is(MENU_MODE));
}

TEST(unknown_key_is_ignored)
{
	hists_prompt_save("abc");
	assert_success(show_prompthistory_menu(&lwin));

	(void)vle_keys_exec(WK_t);
	assert_true(vle_mode_is(MENU_MODE));
}

TEST(fsearch_hist_sets_search_direction_on_pick)
{
	hists_search_save("a");
	assert_success(show_fsearchhistory_menu(&lwin));

	(void)vle_keys_exec(WK_CR);
	assert_false(curr_stats.last_search_backward);
}

TEST(bsearch_hist_sets_search_direction_on_pick)
{
	hists_search_save("a");
	assert_success(show_bsearchhistory_menu(&lwin));

	(void)vle_keys_exec(WK_CR);
	assert_true(curr_stats.last_search_backward);
}

TEST(fsearch_hist_sets_search_direction_on_editing)
{
	hists_search_save("a");
	assert_success(show_fsearchhistory_menu(&lwin));

	(void)vle_keys_exec(WK_c);
	assert_false(curr_stats.last_search_backward);
	(void)vle_keys_exec_timed_out(WK_ESC);
}

TEST(bsearch_hist_sets_search_direction_on_editing)
{
	hists_search_save("a");
	assert_success(show_bsearchhistory_menu(&lwin));

	(void)vle_keys_exec(WK_c);
	assert_true(curr_stats.last_search_backward);
	(void)vle_keys_exec_timed_out(WK_ESC);
}

TEST(editing_search_performs_interactive_search)
{
	cfg.inc_search = 1;
	opt_handlers_setup();

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);
	populate_dir_list(&lwin, 0);
	assert_int_equal(0, lwin.list_pos);

	hists_search_save("var");
	assert_success(show_fsearchhistory_menu(&lwin));

	(void)vle_keys_exec(WK_c);
	assert_int_equal(9, lwin.list_pos);
	(void)vle_keys_exec_timed_out(WK_ESC);
	assert_int_equal(0, lwin.list_pos);

	opt_handlers_teardown();
	cfg.inc_search = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
