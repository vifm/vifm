#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

/* This tests various menus and generic things that need mode activation. */

SETUP()
{
	conf_setup();
	modes_init();
	cmds_init();

	curr_view = &lwin;
	other_view = &rwin;
	view_setup(&lwin);

	curr_stats.load_stage = -1;
	curr_stats.save_msg = 0;
}

TEARDOWN()
{
	vle_keys_reset();
	conf_teardown();

	view_teardown(&lwin);
	curr_view = NULL;
	other_view = NULL;

	curr_stats.load_stage = 0;
}

TEST(enter_loads_selected_colorscheme)
{
	make_abs_path(cfg.colors_dir, sizeof(cfg.colors_dir), TEST_DATA_PATH,
			"color-schemes/", NULL);

	assert_success(cmds_dispatch("colorscheme", &lwin, CIT_COMMAND));

	strcpy(cfg.cs.name, "test-scheme");
	cfg.cs.color[WIN_COLOR].fg = 1;
	cfg.cs.color[WIN_COLOR].bg = 2;
	cfg.cs.color[WIN_COLOR].attr = 3;

	(void)vle_keys_exec(WK_G);
	(void)vle_keys_exec(WK_CR);

	assert_string_equal("good", cfg.cs.name);
	assert_int_equal(-1, cfg.cs.color[WIN_COLOR].fg);
	assert_int_equal(-1, cfg.cs.color[WIN_COLOR].bg);
	assert_int_equal(0, cfg.cs.color[WIN_COLOR].attr);

	(void)vle_keys_exec(WK_ESC);
}

TEST(menu_can_be_searched_interactively)
{
	cfg.inc_search = 1;

	assert_success(cmds_dispatch("vifm", &lwin, CIT_COMMAND));
	menu_data_t *menu = menu_get_current();

	(void)vle_keys_exec_timed_out(L"/^Git info:");
	assert_string_starts_with("Git info:", menu->items[menu->pos]);
	(void)vle_keys_exec_timed_out(WK_C_u L"^Version:");
	assert_string_starts_with("Version:", menu->items[menu->pos]);

	(void)vle_keys_exec_timed_out(WK_ESC);
	(void)vle_keys_exec_timed_out(WK_ESC);
	cfg.inc_search = 0;
}

TEST(menu_is_built_from_a_command)
{
	undo_setup();

	assert_success(cmds_dispatch("!echo only-line %m", &lwin, CIT_COMMAND));

	assert_int_equal(1, menu_get_current()->len);
	assert_string_equal("only-line", menu_get_current()->items[0]);
	assert_string_equal("!echo only-line %m", menu_get_current()->title);

	(void)vle_keys_exec(WK_ESC);
	undo_teardown();
}

TEST(menu_is_built_from_a_command_with_input, IF(have_cat))
{
	undo_setup();

	setup_grid(&lwin, 20, 2, /*init=*/1);
	replace_string(&lwin.dir_entry[0].name, "a");
	replace_string(&lwin.dir_entry[1].name, "b");
	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;
	lwin.pending_marking = 1;

	assert_success(cmds_dispatch("!cat %Pl%m", &lwin, CIT_COMMAND));

	assert_int_equal(2, menu_get_current()->len);
	assert_string_equal("/path/a", menu_get_current()->items[0]);
	assert_string_equal("/path/b", menu_get_current()->items[1]);
	assert_string_equal("!cat %Pl%m", menu_get_current()->title);

	(void)vle_keys_exec(WK_ESC);
	undo_teardown();
}

TEST(menu_cmds_have_a_history)
{
	histories_init(5);
	undo_setup();

	assert_success(cmds_dispatch1("!echo only-line %m", &lwin, CIT_COMMAND));
	(void)vle_keys_exec_timed_out(L":bad1" WK_CR);
	(void)vle_keys_exec_timed_out(L":bad2" WK_CR);

	(void)vle_keys_exec_timed_out(L":");
	line_stats_t *stats = get_line_stats();
	(void)vle_keys_exec_timed_out(WK_C_p);
	assert_wstring_equal(L"bad2", stats->line);
	(void)vle_keys_exec_timed_out(WK_C_p);
	assert_wstring_equal(L"bad1", stats->line);
	(void)vle_keys_exec(WK_ESC);

	(void)vle_keys_exec(WK_ESC);
	undo_teardown();
}

TEST(menu_is_turned_into_cv)
{
	undo_setup();

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);
	assert_success(cmds_dispatch("!echo existing-files/a%M", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_b);
	assert_true(flist_custom_active(&lwin));
	assert_string_equal("!echo existing-files/a%M", lwin.custom.title);

	undo_teardown();
}

TEST(can_not_unstash_a_menu_when_there_is_none)
{
	menus_drop_stash();

	ui_sb_msg("");
	assert_failure(cmds_dispatch1("copen", &lwin, CIT_COMMAND));
	assert_string_equal("No saved menu to display", ui_sb_last());
}

TEST(can_unstash_a_menu)
{
	menus_drop_stash();
	undo_setup();

	assert_success(cmds_dispatch1("!echo only-line %M", &lwin, CIT_COMMAND));

	assert_int_equal(1, menu_get_current()->len);
	assert_string_equal("only-line", menu_get_current()->items[0]);
	(void)vle_keys_exec(WK_ESC);

	assert_success(cmds_dispatch1("copen", &lwin, CIT_COMMAND));

	assert_int_equal(1, menu_get_current()->len);
	assert_string_equal("only-line", menu_get_current()->items[0]);
	(void)vle_keys_exec(WK_ESC);

	undo_teardown();
}

TEST(locate_menu_can_escape_args, IF(not_windows))
{
	char script_path[PATH_MAX + 1];
	make_abs_path(script_path, sizeof(script_path), SANDBOX_PATH, "script", NULL);

	char locate_prg[PATH_MAX + 1];
	snprintf(locate_prg, sizeof(locate_prg), "%s", script_path);
	update_string(&cfg.locate_prg, locate_prg);

	create_executable(SANDBOX_PATH "/script");
	make_file(SANDBOX_PATH "/script",
			"#!/bin/sh\n"
			"for arg; do echo \"$arg\"; done\n");

	assert_success(cmds_dispatch("locate a  b", &lwin, CIT_COMMAND));
	assert_int_equal(1, menu_get_current()->len);
	assert_string_equal("a  b", menu_get_current()->items[0]);

	assert_success(cmds_dispatch("locate -a  b", &lwin, CIT_COMMAND));
	assert_int_equal(2, menu_get_current()->len);
	assert_string_equal("-a", menu_get_current()->items[0]);
	assert_string_equal("b", menu_get_current()->items[1]);

	assert_success(remove(script_path));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
