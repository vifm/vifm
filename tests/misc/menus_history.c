#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
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
#include "../../src/status.h"

static line_stats_t *stats;

SETUP_ONCE()
{
	stats = get_line_stats();
}

SETUP()
{
	enum { HISTORY_SIZE = 10 };

	init_modes();
	init_commands();

	curr_view = &lwin;
	view_setup(&lwin);

	/* Emulate proper history initialization (must happen after view
	 * initialization). */
	cfg_resize_histories(HISTORY_SIZE);
	cfg_resize_histories(0);

	cfg_resize_histories(HISTORY_SIZE);

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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
