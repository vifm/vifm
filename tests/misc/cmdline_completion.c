#include <string.h>

#include "seatest.h"

#include "../../src/cmdline.h"
#include "../../src/commands.h"
#include "../../src/completion.h"
#include "../../src/options.h"

struct line_stats stats;

static void
fusehome_handler(enum opt_op op, union optval_t val)
{
}

static void
setup(void)
{
	static int option_changed;

	stats.line = wcsdup(L"set ");
	stats.index = wcslen(stats.line);
	stats.curs_pos = 0;
	stats.len = stats.index;
	stats.cmd_pos = -1;
	stats.complete_continue = 0;
	stats.history_search = 0;
	stats.line_buf = NULL;

	add_command("bar", "");
	add_command("baz", "");
	add_command("foo", "");

	init_options(&option_changed, NULL);
	add_option("fusehome", "fh", OPT_STR, 0, NULL, fusehome_handler);
}

static void
teardown(void)
{
	free(stats.line);
}

static void
leave_spaces_at_begin(void)
{
	char *buf;

	reset_completion();
	buf = command_completion(" q", 0);
	assert_string_equal(" quit", buf);
	free(buf);
	buf = command_completion(NULL, 0);
	assert_string_equal(" quit", buf);
	free(buf);
}

static void
only_user(void)
{
	char *buf;

	reset_completion();
	buf = command_completion("command ", 1);
	assert_string_equal("command bar", buf);
	free(buf);

	reset_completion();
	buf = command_completion(" command ", 1);
	assert_string_equal(" command bar", buf);
	free(buf);

	reset_completion();
	buf = command_completion("  command ", 1);
	assert_string_equal("  command bar", buf);
	free(buf);
}

static void
test_set_completion(void)
{
	init_commands();
	reset_completion();
	assert_int_equal(0, line_completion(&stats));
	assert_true(wcscmp(stats.line, L"set fusehome") == 0);
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
