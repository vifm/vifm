#include <stic.h>

#include <stddef.h> /* NULL */

#include <test-utils.h>

#include "../../src/engine/keys.h"
#include "../../src/menus/map_menu.h"
#include "../../src/modes/menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"

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

TEST(nop_rhs_is_displayed)
{
	assert_success(cmds_dispatch("nmap lhs <nop>", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("nmap lhs", &lwin, CIT_COMMAND));

	assert_int_equal(4, menu_get_current()->len);
	assert_string_equal("User mappings:", menu_get_current()->items[0]);
	assert_string_equal("lhs         <nop>", menu_get_current()->items[1]);
	assert_string_equal("", menu_get_current()->items[2]);
	assert_string_equal("Builtin mappings:", menu_get_current()->items[3]);

	modes_abort_menu_like();
}

TEST(space_in_rhs_is_displayed_without_notation)
{
	assert_success(cmds_dispatch("nmap lhs s p a c e", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("nmap lhs", &lwin, CIT_COMMAND));

	assert_int_equal(4, menu_get_current()->len);
	assert_string_equal("User mappings:", menu_get_current()->items[0]);
	assert_string_equal("lhs         s p a c e", menu_get_current()->items[1]);
	assert_string_equal("", menu_get_current()->items[2]);
	assert_string_equal("Builtin mappings:", menu_get_current()->items[3]);

	modes_abort_menu_like();
}

TEST(single_space_is_displayed_using_notation)
{
	assert_success(cmds_dispatch("nmap <space> <space>", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("nmap <space>", &lwin, CIT_COMMAND));

	assert_int_equal(5, menu_get_current()->len);
	assert_string_equal("User mappings:", menu_get_current()->items[0]);
	assert_string_equal("<space>     <space>", menu_get_current()->items[1]);
	assert_string_equal("", menu_get_current()->items[2]);
	assert_string_equal("Builtin mappings:", menu_get_current()->items[3]);
	assert_string_equal("<space>     switch pane", menu_get_current()->items[4]);

	modes_abort_menu_like();
}

TEST(first_or_last_space_is_displayed_using_notation)
{
	assert_success(cmds_dispatch("nmap <space>1 <space>1", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("nmap <space>2 <space>2 x", &lwin, CIT_COMMAND));

	assert_success(cmds_dispatch("nmap <space>", &lwin, CIT_COMMAND));
	assert_int_equal(6, menu_get_current()->len);
	assert_string_equal("User mappings:", menu_get_current()->items[0]);
	assert_string_equal("<space>1    <space>1", menu_get_current()->items[1]);
	assert_string_equal("<space>2    <space>2 x", menu_get_current()->items[2]);
	assert_string_equal("", menu_get_current()->items[3]);
	assert_string_equal("Builtin mappings:", menu_get_current()->items[4]);
	assert_string_equal("<space>     switch pane", menu_get_current()->items[5]);

	assert_success(cmds_dispatch("nunmap <space>1", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("nunmap <space>2", &lwin, CIT_COMMAND));

	assert_success(cmds_dispatch("nmap x1<space> 1<space>", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("nmap x2<space> 2 x<space>", &lwin, CIT_COMMAND));

	assert_success(cmds_dispatch("nmap x", &lwin, CIT_COMMAND));
	assert_int_equal(5, menu_get_current()->len);
	assert_string_equal("User mappings:", menu_get_current()->items[0]);
	assert_string_equal("x1<space>   1<space>", menu_get_current()->items[1]);
	assert_string_equal("x2<space>   2 x<space>", menu_get_current()->items[2]);
	assert_string_equal("", menu_get_current()->items[3]);
	assert_string_equal("Builtin mappings:", menu_get_current()->items[4]);

	assert_success(cmds_dispatch("nmap <space>1<space> <space>1<space>",
				&lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("nmap <space>2<space>2<space> <space>2 x<space>",
				&lwin, CIT_COMMAND));

	assert_success(cmds_dispatch("nmap <space>", &lwin, CIT_COMMAND));
	assert_int_equal(6, menu_get_current()->len);
	assert_string_equal("User mappings:", menu_get_current()->items[0]);
	assert_string_equal("<space>1<space> <space>1<space>",
			menu_get_current()->items[1]);
	assert_string_equal("<space>2 2<space> <space>2 x<space>",
			menu_get_current()->items[2]);
	assert_string_equal("", menu_get_current()->items[3]);
	assert_string_equal("Builtin mappings:", menu_get_current()->items[4]);
	assert_string_equal("<space>     switch pane", menu_get_current()->items[5]);

	modes_abort_menu_like();
}

TEST(builtin_key_description_is_displayed)
{
	assert_success(cmds_dispatch("nmap j", &lwin, CIT_COMMAND));

	assert_int_equal(4, menu_get_current()->len);
	assert_string_equal("User mappings:", menu_get_current()->items[0]);
	assert_string_equal("", menu_get_current()->items[1]);
	assert_string_equal("Builtin mappings:", menu_get_current()->items[2]);
	assert_string_equal("j           go to item below",
			menu_get_current()->items[3]);

	modes_abort_menu_like();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
