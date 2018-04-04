#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/cmdline.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"

static line_stats_t *stats;

SETUP_ONCE()
{
	stats = get_line_stats();
	curr_view = &lwin;
}

SETUP()
{
	init_modes();
	enter_cmdline_mode(CLS_COMMAND, "", NULL);
}

TEARDOWN()
{
	(void)vle_keys_exec_timed_out(WK_C_c);

	vle_keys_reset();
}

TEST(unfinished_scope_is_terminated_at_the_end)
{
	(void)vle_keys_exec_timed_out(L":if 'a' == 'b'" WK_C_m);
	assert_true(cmds_scoped_empty());

	(void)vle_keys_exec_timed_out(L":if 'a' == 'a' | | else" WK_C_m);
	assert_true(cmds_scoped_empty());
}

TEST(finished_scope_is_terminated_at_the_end)
{
	(void)vle_keys_exec_timed_out(L":if 'a' == 'a' | else | endif" WK_C_m);
	assert_true(cmds_scoped_empty());
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
