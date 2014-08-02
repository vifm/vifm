#include <unistd.h> /* chdir() */

#include <stddef.h> /* NULL */
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

	assert_int_equal(0, chdir("test-data/existing-files"));
}

static void
teardown(void)
{
	assert_int_equal(0, chdir("../.."));

	free(stats.line);
	reset_cmds();
	clear_options();
}

static void
leave_spaces_at_begin(void)
{
	char *buf;

	reset_completion();
	assert_int_equal(1, complete_cmd(" qui", NULL));
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
	assert_int_equal(8, complete_cmd("command ", NULL));
	buf = next_completion();
	assert_string_equal("bar", buf);
	free(buf);

	reset_completion();
	assert_int_equal(9, complete_cmd(" command ", NULL));
	buf = next_completion();
	assert_string_equal("bar", buf);
	free(buf);

	reset_completion();
	assert_int_equal(10, complete_cmd("  command ", NULL));
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

static void
test_no_sdquoted_completion_does_nothing(void)
{
	free(stats.line);
	stats.line = wcsdup(L"command '");
	stats.len = wcslen(stats.line);
	stats.index = stats.len;

	reset_completion();
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"command '"));
}

static void
prepare_for_line_completion(const wchar_t str[])
{
	free(stats.line);
	stats.line = wcsdup(str);
	stats.len = wcslen(stats.line);
	stats.index = stats.len;

	reset_completion();
}

static void
test_squoted_completion(void)
{
	prepare_for_line_completion(L"touch '");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"touch 'a"));
}

static void
test_squoted_completion_escaping(void)
{
	assert_int_equal(0, chdir("../quotes-in-names"));

	prepare_for_line_completion(L"touch 's-quote");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"touch 's-quote-''-in-name"));
}

static void
test_dquoted_completion(void)
{
	prepare_for_line_completion(L"touch 'b");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"touch 'b"));
}

static void
test_dquoted_completion_escaping(void)
{
	assert_int_equal(0, chdir("../quotes-in-names"));

	prepare_for_line_completion(L"touch \"d-quote");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"touch \"d-quote-\\\"-in-name"));
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
	run_test(test_no_sdquoted_completion_does_nothing);
	run_test(test_squoted_completion);
	run_test(test_squoted_completion_escaping);
	run_test(test_dquoted_completion);
	run_test(test_dquoted_completion_escaping);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
