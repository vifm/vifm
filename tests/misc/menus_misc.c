#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* strcpy() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/status.h"

SETUP()
{
	conf_setup();
	init_modes();
	init_commands();

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

	assert_success(exec_commands("colorscheme", &lwin, CIT_COMMAND));

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

TEST(menu_is_built_from_a_command)
{
	undo_setup();

	assert_success(exec_commands("!echo only-line %m", &lwin, CIT_COMMAND));

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

	assert_success(exec_commands("!cat %Pl%m", &lwin, CIT_COMMAND));

	assert_int_equal(2, menu_get_current()->len);
	assert_string_equal("/path/a", menu_get_current()->items[0]);
	assert_string_equal("/path/b", menu_get_current()->items[1]);
	assert_string_equal("!cat %Pl%m", menu_get_current()->title);

	(void)vle_keys_exec(WK_ESC);
	undo_teardown();
}

TEST(menu_is_turned_into_cv)
{
	undo_setup();

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);
	assert_success(exec_commands("!echo existing-files/a%M", &lwin, CIT_COMMAND));

	(void)vle_keys_exec(WK_b);
	assert_true(flist_custom_active(&lwin));
	assert_string_equal("!echo existing-files/a%M", lwin.custom.title);

	undo_teardown();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
