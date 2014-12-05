#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */
#include <wchar.h> /* wcsdup() wcslen() */

#include "seatest.h"

#include "../../src/engine/cmds.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/macros.h"
#include "../../src/commands.h"
#include "../../src/status.h"

static int builtin_cmd(const cmd_info_t* cmd_info);

static const cmd_add_t commands[] =
{
	{ .name = "builtin", .abbr = NULL, .handler = builtin_cmd, .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 1,        .qmark = 0,   .expand = 0,            .regexp = 0, .min_args = 0, .max_args = 0, .bg = 0, },
};

static int called;

static void
setup(void)
{
	lwin.list_rows = 0;

	curr_view = &lwin;

	init_commands();

	add_builtin_commands(commands, ARRAY_LEN(commands));
}

static void
teardown(void)
{
	reset_cmds();
}

static int
builtin_cmd(const cmd_info_t* cmd_info)
{
	called = 1;
	return 0;
}

static void
test_repeat_of_no_command_prints_message(void)
{
	called = 0;
	(void)exec_commands("builtin", &lwin, GET_COMMAND);
	assert_int_equal(1, called);

	assert_string_equal(NULL, curr_stats.last_cmdline_command);

	called = 0;
	assert_int_equal(1, exec_commands("!!", &lwin, GET_COMMAND));
	assert_int_equal(0, called);
}

static void
test_double_emark_repeats_last_command(void)
{
	called = 0;
	(void)exec_commands("builtin", &lwin, GET_COMMAND);
	assert_int_equal(1, called);

	free(curr_stats.last_cmdline_command);
	curr_stats.last_cmdline_command = strdup("builtin");

	called = 0;
	assert_int_equal(0, exec_commands("!!", &lwin, GET_COMMAND));
	assert_int_equal(1, called);

	free(curr_stats.last_cmdline_command);
	curr_stats.last_cmdline_command = NULL;
}

static void
test_single_emark_without_args_fails(void)
{
	free(curr_stats.last_cmdline_command);
	curr_stats.last_cmdline_command = strdup("builtin");

	called = 0;
	assert_false(exec_commands("!", &lwin, GET_COMMAND) == 0);
	assert_int_equal(0, called);

	free(curr_stats.last_cmdline_command);
	curr_stats.last_cmdline_command = NULL;
}

void
cmdline_emark_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_repeat_of_no_command_prints_message);
	run_test(test_double_emark_repeats_last_command);
	run_test(test_single_emark_without_args_fails);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
