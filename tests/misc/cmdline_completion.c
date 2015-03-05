#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <string.h>
#include <wchar.h> /* wcsdup() */

#include "../../src/cfg/config.h"
#include "../../src/engine/abbrevs.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/engine/functions.h"
#include "../../src/engine/options.h"
#include "../../src/modes/cmdline.h"
#include "../../src/utils/str.h"
#include "../../src/builtin_functions.h"
#include "../../src/commands.h"

static line_stats_t stats;

static void
fusehome_handler(OPT_OP op, optval_t val)
{
}

SETUP()
{
	static int option_changed;
	optval_t def = { .str_val = "/tmp" };

	init_builtin_functions();

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

TEARDOWN()
{
	assert_int_equal(0, chdir("../.."));

	free(stats.line);
	reset_cmds();
	clear_options();

	function_reset_all();
}

TEST(leave_spaces_at_begin)
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

TEST(only_user)
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

TEST(test_set_completion)
{
	vle_compl_reset();
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"set all", stats.line);
}

TEST(no_sdquoted_completion_does_nothing)
{
	free(stats.line);
	stats.line = wcsdup(L"command '");
	stats.len = wcslen(stats.line);
	stats.index = stats.len;

	vle_compl_reset();
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"command '", stats.line);
}

static void
prepare_for_line_completion(const wchar_t str[])
{
	free(stats.line);
	stats.line = wcsdup(str);
	stats.len = wcslen(stats.line);
	stats.index = stats.len;
	stats.complete_continue = 0;

	vle_compl_reset();
}

TEST(spaces_escaping_leading)
{
	char *mb;

	assert_int_equal(0, chdir("../spaces-in-names"));

	prepare_for_line_completion(L"touch \\ ");
	assert_success(line_completion(&stats));

	mb = to_multibyte(stats.line);
	assert_string_equal("touch \\ begins-with-space", mb);
	free(mb);
}

TEST(spaces_escaping_everywhere)
{
	char *mb;

	assert_int_equal(0, chdir("../spaces-in-names"));

	prepare_for_line_completion(L"touch \\ s");
	assert_success(line_completion(&stats));

	mb = to_multibyte(stats.line);
#ifndef _WIN32
	assert_string_equal("touch \\ spaces\\ everywhere\\ ", mb);
#else
	/* Files can't have trailing whitespace on Windows. */
	assert_string_equal("touch \\ spaces\\ everywhere", mb);
#endif
	free(mb);
}

TEST(spaces_escaping_trailing)
{
	char *mb;

	assert_int_equal(0, chdir("../spaces-in-names"));

	prepare_for_line_completion(L"touch e");
	assert_success(line_completion(&stats));

	mb = to_multibyte(stats.line);
#ifndef _WIN32
	assert_string_equal("touch ends-with-space\\ ", mb);
#else
	/* Files can't have trailing whitespace on Windows. */
	assert_string_equal("touch ends-with-space", mb);
#endif
	free(mb);
}

TEST(spaces_escaping_middle)
{
	char *mb;

	assert_int_equal(0, chdir("../spaces-in-names"));

	prepare_for_line_completion(L"touch s");
	assert_success(line_completion(&stats));

	mb = to_multibyte(stats.line);
	assert_string_equal("touch spaces\\ in\\ the\\ middle", mb);
	free(mb);
}

TEST(squoted_completion)
{
	prepare_for_line_completion(L"touch '");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"touch 'a", stats.line);
}

TEST(squoted_completion_escaping)
{
	assert_int_equal(0, chdir("../quotes-in-names"));

	prepare_for_line_completion(L"touch 's-quote");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"touch 's-quote-''-in-name", stats.line);
}

TEST(dquoted_completion)
{
	prepare_for_line_completion(L"touch 'b");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"touch 'b", stats.line);
}

#if !defined(__CYGWIN__) && !defined(_WIN32)

TEST(dquoted_completion_escaping)
{
	assert_int_equal(0, chdir("../quotes-in-names"));

	prepare_for_line_completion(L"touch \"d-quote");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"touch \"d-quote-\\\"-in-name", stats.line);
}

#endif

TEST(last_match_is_properly_escaped)
{
	char *match;

	assert_int_equal(0, chdir("../quotes-in-names"));

	prepare_for_line_completion(L"touch 's-quote-''-in");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"touch 's-quote-''-in-name", stats.line);

	match = vle_compl_next();
	assert_string_equal("s-quote-''-in-name-2", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("s-quote-''-in", match);
	free(match);
}

TEST(emark_cmd_escaping)
{
	char *match;

	prepare_for_line_completion(L"");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"!", stats.line);

	match = vle_compl_next();
	assert_string_equal("alink", match);
	free(match);
}

TEST(winrun_cmd_escaping)
{
	char *match;

	prepare_for_line_completion(L"winrun ");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"winrun $", stats.line);

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

TEST(help_cmd_escaping)
{
	cfg.use_vim_help = 1;

	prepare_for_line_completion(L"help vifm-");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"help vifm-!!", stats.line);
}

TEST(dirs_are_completed_with_trailing_slash)
{
	char *match;

	assert_int_equal(0, chdir("../"));

	prepare_for_line_completion(L"cd r");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"cd read/", stats.line);

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

TEST(function_name_completion)
{
	char *match;

	prepare_for_line_completion(L"echo e");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"echo executable(", stats.line);

	match = vle_compl_next();
	assert_string_equal("expand(", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("e", match);
	free(match);
}

TEST(percent_completion)
{
	char *match;

	/* One percent symbol. */

	prepare_for_line_completion(L"cd %");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"cd %%", stats.line);

	match = vle_compl_next();
	assert_string_equal("%%", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("%%", match);
	free(match);

	/* Two percent symbols. */

	prepare_for_line_completion(L"cd %%");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"cd %%", stats.line);

	match = vle_compl_next();
	assert_string_equal("%%", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("%%", match);
	free(match);

	/* Three percent symbols. */

	prepare_for_line_completion(L"cd %%%");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"cd %%%%", stats.line);

	match = vle_compl_next();
	assert_string_equal("%%%%", match);
	free(match);

	match = vle_compl_next();
	assert_string_equal("%%%%", match);
	free(match);
}

TEST(abbreviations)
{
	vle_abbr_reset();
	assert_success(vle_abbr_add(L"lhs", L"rhs"));

	prepare_for_line_completion(L"cabbrev l");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"cabbrev lhs", stats.line);

	prepare_for_line_completion(L"cnoreabbrev l");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"cnoreabbrev lhs", stats.line);

	prepare_for_line_completion(L"cunabbrev l");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"cunabbrev lhs", stats.line);

	prepare_for_line_completion(L"cabbrev l l");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"cabbrev l l", stats.line);

	vle_abbr_reset();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
