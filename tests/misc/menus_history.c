#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/menus/history_menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
