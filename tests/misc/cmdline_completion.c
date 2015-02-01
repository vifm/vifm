#include <unistd.h> /* chdir() */

#include <stddef.h> /* NULL */
#include <stdlib.h>
#include <string.h>
#include <wchar.h> /* wcsdup() */

#include "seatest.h"

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/engine/functions.h"
#include "../../src/engine/options.h"
#include "../../src/modes/cmdline.h"
#include "../../src/builtin_functions.h"
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

	vle_compl_reset();
	assert_int_equal(1, complete_cmd(" qui", NULL));
	buf = vle_compl_next();
	assert_string_equal("quit", buf);
	free(buf);
	buf = vle_compl_next();
	assert_string_equal("quit", buf);
	free(buf);
}

static void
only_user(void)
{
	char *buf;

	vle_compl_reset();
	assert_int_equal(8, complete_cmd("command ", NULL));
	buf = vle_compl_next();
	assert_string_equal("bar", buf);
	free(buf);

	vle_compl_reset();
	assert_int_equal(9, complete_cmd(" command ", NULL));
	buf = vle_compl_next();
	assert_string_equal("bar", buf);
	free(buf);

	vle_compl_reset();
	assert_int_equal(10, complete_cmd("  command ", NULL));
	buf = vle_compl_next();
	assert_string_equal("bar", buf);
	free(buf);
}

static void
test_set_completion(void)
{
	vle_compl_reset();
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

	vle_compl_reset();
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

	vle_compl_reset();
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

#if !defined(__CYGWIN__) && !defined(_WIN32)

static void
test_dquoted_completion_escaping(void)
{
	assert_int_equal(0, chdir("../quotes-in-names"));

	prepare_for_line_completion(L"touch \"d-quote");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"touch \"d-quote-\\\"-in-name"));
}

#endif

static void
test_last_match_is_properly_escaped(void)
{
	char *match;

	assert_int_equal(0, chdir("../quotes-in-names"));

	prepare_for_line_completion(L"touch 's-quote-''-in");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"touch 's-quote-''-in-name"));

	match = vle_compl_next();
	assert_string_equal("s-quote-''-in-name-2", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("s-quote-''-in", match);
	free(match);
}

static void
test_emark_cmd_escaping(void)
{
	char *match;

	prepare_for_line_completion(L"");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"!"));

	match = vle_compl_next();
	assert_string_equal("alink", match);
	free(match);
}

static void
test_winrun_cmd_escaping(void)
{
	char *match;

	prepare_for_line_completion(L"winrun ");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"winrun $"));

	match = vle_compl_next();
	assert_string_equal("%", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal(",", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal(".", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("^", match);
	free(match);
}

static void
test_help_cmd_escaping(void)
{
	cfg.use_vim_help = 1;

	prepare_for_line_completion(L"help vifm-");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"help vifm-!!"));
}

static void
test_dirs_are_completed_with_trailing_slash(void)
{
	char *match;

	assert_int_equal(0, chdir("../"));

	prepare_for_line_completion(L"cd r");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"cd read/"));

	match = vle_compl_next();
	assert_string_equal("rename/", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("r", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("read/", match);
	free(match);

	assert_int_equal(0, chdir("read/"));
}

static void
test_function_name_completion(void)
{
	char *match;

	prepare_for_line_completion(L"echo e");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"echo executable("));

	match = vle_compl_next();
	assert_string_equal("expand(", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("e", match);
	free(match);
}

static void
test_percent_completion(void)
{
	char *match;

	/* One percent symbol. */

	prepare_for_line_completion(L"cd %");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"cd %%"));

	match = vle_compl_next();
	assert_string_equal("%%", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("%%", match);
	free(match);

	/* Two percent symbols. */

	prepare_for_line_completion(L"cd %%");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"cd %%"));

	match = vle_compl_next();
	assert_string_equal("%%", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("%%", match);
	free(match);

	/* Three percent symbols. */

	prepare_for_line_completion(L"cd %%%");
	assert_int_equal(0, line_completion(&stats));
	assert_int_equal(0, wcscmp(stats.line, L"cd %%%%"));

	match = vle_compl_next();
	assert_string_equal("%%%%", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("%%%%", match);
	free(match);
}

void
test_cmdline_completion(void)
{
	test_fixture_start();

	init_builtin_functions();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(leave_spaces_at_begin);
	run_test(only_user);
	run_test(test_set_completion);
	run_test(test_no_sdquoted_completion_does_nothing);
	run_test(test_squoted_completion);
	run_test(test_squoted_completion_escaping);
	run_test(test_dquoted_completion);
	run_test(test_last_match_is_properly_escaped);
	run_test(test_emark_cmd_escaping);
	run_test(test_winrun_cmd_escaping);
	run_test(test_help_cmd_escaping);
	run_test(test_dirs_are_completed_with_trailing_slash);
	run_test(test_function_name_completion);
	run_test(test_percent_completion);
#if !defined(__CYGWIN__) && !defined(_WIN32)
	/* Cygwin and Windows fail to create files with double quotes in names. */
	run_test(test_dquoted_completion_escaping);
#endif

	function_reset_all();

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
