#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/menus/registers_menu.h"
#include "../../src/modes/menu.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/registers.h"
#include "../../src/status.h"

SETUP()
{
	regs_init();
	modes_init();

	curr_stats.load_stage = -1;
}

TEARDOWN()
{
	(void)vle_keys_exec(WK_ESC);

	vle_keys_reset();
	regs_reset();

	curr_stats.load_stage = 0;
}

TEST(empty_registers_menu_is_not_displayed)
{
	ui_sb_msg("");
	assert_failure(show_register_menu(&lwin, "a"));
	assert_string_equal("Registers are empty", ui_sb_last());
}

TEST(registers_menu)
{
	regs_append('a', "1");
	regs_append('a', "2");

	assert_success(show_register_menu(&lwin, "a"));

	assert_int_equal(3, menu_get_current()->len);
	assert_string_equal("\"a", menu_get_current()->items[0]);
	assert_string_equal("1", menu_get_current()->items[1]);
	assert_string_equal("2", menu_get_current()->items[2]);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
