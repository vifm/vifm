#include <stic.h>

#include "../../src/utils/env.h"
#include "../../src/engine/cmds.h"
#include "../../src/cmd_core.h"

#include "utils.h"

TEST(whole_line_comments)
{
	assert_success(exec_command("\"", NULL, CIT_COMMAND));
	assert_success(exec_command(" \"", NULL, CIT_COMMAND));
	assert_success(exec_command("  \"", NULL, CIT_COMMAND));
}

TEST(trailing_comments)
{
	view_setup(&lwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;

	init_commands();

	assert_success(exec_command("let $a = 4 \"", &lwin, CIT_COMMAND));
	assert_string_equal("4", env_get("a"));
	assert_success(exec_command("unlet $a \"comment", &lwin, CIT_COMMAND));
	assert_string_equal(NULL, env_get("a"));

	reset_cmds();

	view_teardown(&lwin);
	view_teardown(&rwin);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
