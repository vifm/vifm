#include <stic.h>

#include <ctype.h> /* isspace() */
#include <stddef.h> /* NULL */
#include <string.h> /* memset() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/abbrevs.h"
#include "../../src/menus/cabbrevs_menu.h"
#include "../../src/modes/menu.h"
#include "../../src/modes/modes.h"
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
	modes_abort_menu_like();

	vle_abbr_reset();
	conf_teardown();

	view_teardown(&lwin);
	curr_view = NULL;
	other_view = NULL;

	curr_stats.load_stage = 0;
}

TEST(title_and_no_remap)
{
	assert_success(vle_abbr_add_no_remap(L"lhs1", L"rhs1"));
	assert_success(vle_abbr_add(L"lhs2", L"rhs2"));

	assert_success(cmds_dispatch("cabbrev", &lwin, CIT_COMMAND));

	menu_data_t *m = menu_get_current();
	assert_int_equal(2, m->len);
	assert_string_equal("Abbreviation -- N -- Replacement", m->title);
	assert_string_equal("lhs1              *    rhs1", m->items[0]);
	assert_string_equal("lhs2                   rhs2", m->items[1]);
}

TEST(bracket_notation_in_rhs)
{
	int i;
	for(i = 0; i < 255; ++i)
	{
		cfg.word_chars[i] = !isspace(i);
	}

	assert_success(cmds_dispatch("cnoreabbrev lhs <lt>", &lwin, CIT_COMMAND));

	assert_success(cmds_dispatch("cabbrev", &lwin, CIT_COMMAND));

	menu_data_t *m = menu_get_current();
	assert_int_equal(1, m->len);
	assert_string_equal("lhs               *    <", m->items[0]);

	memset(cfg.word_chars, 0, sizeof(cfg.word_chars));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
