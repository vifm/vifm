#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/functions.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/env.h"
#include "../../src/builtin_functions.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

SETUP()
{
	init_modes();

	view_setup(&lwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	opt_handlers_setup();

	init_commands();
}

TEARDOWN()
{
	opt_handlers_teardown();
	vle_keys_reset();
	vle_cmds_reset();

	view_teardown(&lwin);
	view_teardown(&rwin);
}

TEST(wincmd_can_switch_views)
{
	assert_success(stats_init(&cfg));

	curr_view = &rwin;
	other_view = &lwin;
	assert_success(exec_commands("wincmd h", curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);

	curr_view = &rwin;
	other_view = &lwin;
	assert_success(exec_commands("execute 'wincmd h'", curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);

	init_builtin_functions();

	curr_view = &rwin;
	other_view = &lwin;
	assert_success(
			exec_commands("if paneisat('left') == 0 | execute 'wincmd h' | endif",
				curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);

	curr_view = &rwin;
	other_view = &lwin;
	assert_success(
			exec_commands("if paneisat('left') == 0 "
			             "|    execute 'wincmd h' "
			             "|    let $a = paneisat('left') "
			             "|endif",
				curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);
	assert_string_equal("1", env_get("a"));

	function_reset_all();

	assert_success(stats_reset(&cfg));
}

TEST(wincmd_ignores_mappings)
{
	curr_view = &rwin;
	other_view = &lwin;
	assert_success(exec_commands("nnoremap <c-w> <nop>", curr_view, CIT_COMMAND));
	assert_success(exec_commands("wincmd H", curr_view, CIT_COMMAND));
	assert_true(curr_view == &lwin);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
