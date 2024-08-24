#include <stic/stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/ui/ui.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/menu.h"
#include "../../src/cmd_core.h"
#include "../../src/flist_hist.h"
#include "../../src/status.h"

SETUP()
{
	modes_init();
	cmds_init();

	curr_view = &lwin;
	view_setup(&lwin);
	init_view_list(&lwin);

	histories_init(/*size=*/5);
	hists_commands_save("cmd");
	hists_menucmd_save("mcmd");
	hists_exprreg_save("ereg");
	hists_search_save("search");
	hists_prompt_save("prompt");
	hists_filter_save("filter");
	flist_hist_setup(&lwin, "/", NULL, /*rel_pos=*/0, /*timestamp=*/1);

	curr_stats.load_stage = -1;
}

TEARDOWN()
{
	curr_stats.load_stage = 0;

	view_teardown(&lwin);
	curr_view = NULL;

	vle_cmds_reset();
	vle_keys_reset();

	cfg_resize_histories(0);
}

TEST(history_symbols)
{
	assert_success(cmds_dispatch1("history :", &lwin, CIT_COMMAND));
	assert_string_equal("Command Line History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history /", &lwin, CIT_COMMAND));
	assert_string_equal("Search History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history @", &lwin, CIT_COMMAND));
	assert_string_equal("Prompt History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history ?", &lwin, CIT_COMMAND));
	assert_string_equal("Search History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history =", &lwin, CIT_COMMAND));
	assert_string_equal("Filter History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history .", &lwin, CIT_COMMAND));
	assert_string_equal("Directory History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));
}

TEST(history_keywords)
{
	assert_success(cmds_dispatch1("history cmd", &lwin, CIT_COMMAND));
	assert_string_equal("Command Line History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history search", &lwin, CIT_COMMAND));
	assert_string_equal("Search History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history fsearch", &lwin, CIT_COMMAND));
	assert_string_equal("Search History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history input", &lwin, CIT_COMMAND));
	assert_string_equal("Prompt History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history bsearch", &lwin, CIT_COMMAND));
	assert_string_equal("Search History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history filter", &lwin, CIT_COMMAND));
	assert_string_equal("Filter History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history dir", &lwin, CIT_COMMAND));
	assert_string_equal("Directory History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history exprreg", &lwin, CIT_COMMAND));
	assert_string_equal("Expression Register History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history mcmd", &lwin, CIT_COMMAND));
	assert_string_equal("Menu Command History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));
}

TEST(history_shortcuts)
{
	assert_success(cmds_dispatch1("history c", &lwin, CIT_COMMAND));
	assert_string_equal("Command Line History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history s", &lwin, CIT_COMMAND));
	assert_string_equal("Search History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history f", &lwin, CIT_COMMAND));
	assert_string_equal("Search History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history i", &lwin, CIT_COMMAND));
	assert_string_equal("Prompt History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history b", &lwin, CIT_COMMAND));
	assert_string_equal("Search History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history fi", &lwin, CIT_COMMAND));
	assert_string_equal("Filter History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history d", &lwin, CIT_COMMAND));
	assert_string_equal("Directory History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history e", &lwin, CIT_COMMAND));
	assert_string_equal("Expression Register History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));

	assert_success(cmds_dispatch1("history mc", &lwin, CIT_COMMAND));
	assert_string_equal("Menu Command History", menu_get_current()->title);
	assert_success(cmds_dispatch1("q", &lwin, CIT_MENU_COMMAND));
}

TEST(history_bad_param)
{
	assert_failure(cmds_dispatch1("history x", &lwin, CIT_COMMAND));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
