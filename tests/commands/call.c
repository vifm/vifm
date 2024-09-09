#include <stic.h>

#include "../../src/cmd_core.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/builtin_functions.h"

#include <test-utils.h>

static void check_and_remove_file(void);

SETUP_ONCE()
{
	vle_parser_init(NULL);
	init_builtin_functions();
	cmds_init();
	conf_setup();
}

TEARDOWN_ONCE()
{
	function_reset_all();
	vle_cmds_reset();
	conf_teardown();
}

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN()
{
	curr_view = NULL;
	other_view = NULL;
}

TEST(good_call)
{
	assert_success(cmds_dispatch1("call system('echo call> file')", &lwin,
				CIT_COMMAND));
	check_and_remove_file();
}

TEST(good_call_with_comment)
{
	assert_success(cmds_dispatch1("call system('echo call> file') \" comment",
				&lwin, CIT_COMMAND));
	check_and_remove_file();
}

TEST(unknown_function)
{
	ui_sb_msg("");
	assert_failure(cmds_dispatch1("call nosuchfunc()", &lwin, CIT_COMMAND));
	assert_string_equal("Invalid expression: nosuchfunc()", ui_sb_last());
}

TEST(bad_expression)
{
	ui_sb_msg("");
	assert_failure(cmds_dispatch1("call 1 + 2", &lwin, CIT_COMMAND));
	assert_string_equal("Invalid expression: 1 + 2", ui_sb_last());
}

TEST(side_effects_on_error)
{
	ui_sb_msg("");
	assert_failure(cmds_dispatch1("call 2 + system('echo call> file')",
				&lwin, CIT_COMMAND));
	assert_string_equal("Invalid expression: 2 + system('echo call> file')",
			ui_sb_last());
	no_remove_file("file");

	/* Unlike in Vim, don't execute call expression followed by trailing
	 * characters. */
	ui_sb_msg("");
	assert_failure(cmds_dispatch1("call system('echo call> file') + 2",
				&lwin, CIT_COMMAND));
	assert_string_equal("Invalid expression: system('echo call> file') + 2",
			ui_sb_last());
	no_remove_file("file");
}

static void
check_and_remove_file(void)
{
	const char *lines[] = { "call" };
	file_is("file", lines, 1);

	remove_file("file");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
