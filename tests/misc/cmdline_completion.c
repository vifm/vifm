#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/engine/options.h"
#include "../../src/modes/cmdline.h"
#include "../../src/commands.h"

line_stats_t stats;

static void
fusehome_handler(OPT_OP op, optval_t val)
{
}

static void
setup(void)
{
	static int option_changed;
	optval_t def = { .str_val = "/tmp" };

	stats.line = wcsdup(L"set ");
	stats.index = wcslen(stats.line);
	stats.curs_pos = 0;
	stats.len = stats.index;
	stats.cmd_pos = -1;
	stats.complete_continue = 0;
	stats.history_search = 0;
	stats.line_buf = NULL;
	stats.complete = &complete_cmd;

	init_commands();

	execute_cmd("command bar a");
	execute_cmd("command baz b");
	execute_cmd("command foo c");

	init_options(&option_changed);
	add_option("fusehome", "fh", OPT_STR, 0, NULL, fusehome_handler, def);
}

static void
teardown(void)
{
	free(stats.line);
	reset_cmds();
	clear_options();
}

static void
leave_spaces_at_begin(void)
{
	char *buf;

	reset_completion();
	assert_int_equal(1, complete_cmd(" qui"));
	buf = next_completion();
	assert_string_equal("quit", buf);
	free(buf);
	buf = next_completion();
	assert_string_equal("quit", buf);
	free(buf);
}

static void
only_user(void)
{
	char *buf;

	reset_completion();
	assert_int_equal(8, complete_cmd("command "));
	buf = next_completion();
	assert_string_equal("bar", buf);
	free(buf);

	reset_completion();
	assert_int_equal(9, complete_cmd(" command "));
	buf = next_completion();
	assert_string_equal("bar", buf);
	free(buf);

	reset_completion();
	assert_int_equal(10, complete_cmd("  command "));
	buf = next_completion();
	assert_string_equal("bar", buf);
	free(buf);
}

static void
test_set_completion(void)
{
	reset_completion();
	assert_int_equal(0, line_completion(&stats));
	assert_true(wcscmp(stats.line, L"set all") == 0);
}

void
test_cmdline_completion(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(leave_spaces_at_begin);
	run_test(only_user);
	run_test(test_set_completion);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
