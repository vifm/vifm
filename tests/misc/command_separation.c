#include <stic.h>

#include <limits.h> /* INT_MAX */
#include <stdlib.h> /* free() */

#include "../../src/engine/cmds.h"
#include "../../src/utils/string_array.h"
#include "../../src/cmd_core.h"

SETUP()
{
	init_commands();
}

TEARDOWN()
{
	vle_cmds_reset();
}

TEST(pipe)
{
	const char *buf;

	buf = "filter /a|b/";
	assert_int_equal(0, line_pos(buf, buf, ' ', 1, 1));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 1, 1));
	assert_int_equal(5, line_pos(buf, buf + 9, ' ', 1, 1));

	buf = "filter 'a|b'";
	assert_int_equal(0, line_pos(buf, buf, ' ', 1, 1));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 1, 1));
	assert_int_equal(3, line_pos(buf, buf + 9, ' ', 1, 1));

	buf = "filter a|b";
	assert_int_equal(0, line_pos(buf, buf, ' ', 1, 1));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 1, 1));
	assert_int_equal(5, line_pos(buf, buf + 8, ' ', 1, 1));
	assert_int_equal(5, line_pos(buf, buf + 9, ' ', 1, 1));
	assert_int_equal(5, line_pos(buf, buf + 10, ' ', 1, 1));

	buf = "filter \"a|b\"";
	assert_int_equal(0, line_pos(buf, buf, ' ', 1, 1));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 1, 1));
	assert_int_equal(4, line_pos(buf, buf + 9, ' ', 1, 1));

	buf = "filter!/a|b/";
	assert_int_equal(0, line_pos(buf, buf, ' ', 1, 1));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 1, 1));
	assert_int_equal(5, line_pos(buf, buf + 9, ' ', 1, 1));

	buf = "filter /a|b/ |";
	assert_int_equal(0, line_pos(buf, buf, ' ', 1, 1));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 1, 1));
	assert_int_equal(0, line_pos(buf, buf + 13, ' ', 1, 1));
	assert_int_equal(2, line_pos(buf, buf + 14, ' ', 1, 1));

	buf = "select /a|b/ |";
	assert_int_equal(0, line_pos(buf, buf, ' ', 2, INT_MAX));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 2, INT_MAX));
	assert_int_equal(0, line_pos(buf, buf + 13, ' ', 2, INT_MAX));
	assert_int_equal(2, line_pos(buf, buf + 14, ' ', 2, INT_MAX));
}

TEST(two_commands)
{
	const char buf[] = "apropos|locate";

	assert_int_equal(0, line_pos(buf, buf, ' ', 0, 1));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 0, 1));
	assert_int_equal(0, line_pos(buf, buf + 7, ' ', 0, 1));
}

TEST(set_command)
{
	const char *buf;

	buf = "set fusehome=\"a|b\"";
	assert_int_equal(0, line_pos(buf, buf, ' ', 0, INT_MAX));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 0, INT_MAX));
	assert_int_equal(4, line_pos(buf, buf + 16, ' ', 0, INT_MAX));

	buf = "set fusehome='a|b'";
	assert_int_equal(0, line_pos(buf, buf, ' ', 0, INT_MAX));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 0, INT_MAX));
	assert_int_equal(3, line_pos(buf, buf + 16, ' ', 0, INT_MAX));
}

TEST(skip)
{
	const char *buf;

	buf = "set fusehome=a\\|b";
	assert_int_equal(0, line_pos(buf, buf, ' ', 0, INT_MAX));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 0, INT_MAX));
	assert_int_equal(1, line_pos(buf, buf + 15, ' ', 0, INT_MAX));
}

TEST(custom_separator)
{
	const char *buf;

	/*     00000 0000011111 */
	/*     01234 5678901234 */
	buf = "s/a|b\\/c/d|e/g|";
	assert_int_equal(0, line_pos(buf, buf, '/', -3, 3));
	assert_int_equal(0, line_pos(buf, buf + 1, '/', -3, 3));
	assert_int_equal(5, line_pos(buf, buf + 2, '/', -3, 3));
	assert_int_equal(5, line_pos(buf, buf + 3, '/', -3, 3));
	assert_int_equal(5, line_pos(buf, buf + 4, '/', -3, 3));
	assert_int_equal(1, line_pos(buf, buf + 6, '/', -3, 3));
	assert_int_equal(5, line_pos(buf, buf + 10, '/', -3, 3));
	assert_int_equal(2, line_pos(buf, buf + 14, '/', -3, 3));

	/*     00000 0000011111 */
	/*     01234 5678901234 */
	buf = "s!a|b\\/c!d|e!g|";
	assert_int_equal(0, line_pos(buf, buf, '!', -3, 3));
	assert_int_equal(0, line_pos(buf, buf + 1, '!', -3, 3));
	assert_int_equal(5, line_pos(buf, buf + 2, '!', -3, 3));
	assert_int_equal(5, line_pos(buf, buf + 3, '!', -3, 3));
	assert_int_equal(5, line_pos(buf, buf + 4, '!', -3, 3));
	assert_int_equal(1, line_pos(buf, buf + 6, '!', -3, 3));
	assert_int_equal(5, line_pos(buf, buf + 10, '!', -3, 3));
	assert_int_equal(2, line_pos(buf, buf + 14, '!', -3, 3));
}

TEST(space_amp_before_bar)
{
	const char buf[] = "apropos &|locate";

	assert_int_equal(0, line_pos(buf, buf, ' ', 0, 1));
	assert_int_equal(0, line_pos(buf, buf + 7, ' ', 0, 1));
	assert_int_equal(0, line_pos(buf, buf + 8, ' ', 0, 1));
	assert_int_equal(0, line_pos(buf, buf + 9, ' ', 0, 1));
}

TEST(whole_line_command_cmdline_is_not_broken)
{
	char **cmds = break_cmdline("!echo hi|less", 0);

	assert_string_equal("!echo hi|less", cmds[0]);
	assert_string_equal(NULL, cmds[1]);

	free_string_array(cmds, count_strings(cmds));
}

TEST(bar_is_skipped_when_not_surrounded_with_spaces)
{
	char **cmds = break_cmdline("let $a = paneisat('left')|endif", 0);

	assert_string_equal("let $a = paneisat('left')", cmds[0]);
	assert_string_equal("endif", cmds[1]);
	assert_string_equal(NULL, cmds[2]);

	free_string_array(cmds, count_strings(cmds));
}

TEST(bar_escaping_is_preserved_for_whole_line_commands)
{
	char **cmds = break_cmdline("!\\|\\||\\|\\|", 0);

	assert_string_equal("!\\|\\||\\|\\|", cmds[0]);
	assert_string_equal(NULL, cmds[1]);

	free_string_array(cmds, count_strings(cmds));
}

TEST(bar_escaping_is_preserved_for_expression_commands)
{
	char **cmds = break_cmdline("echo 1 \\|| 2", 0);

	assert_string_equal("echo 1 \\|| 2", cmds[0]);
	assert_string_equal(NULL, cmds[1]);

	free_string_array(cmds, count_strings(cmds));
}

TEST(comments_and_bar)
{
	/* XXX: this behaviour is partially Vim-like, it also doesn't break the line
	 *      at bar, but expression parser later errors on "..., which is not the
	 *      case here. */

	char **cmds = break_cmdline("echo 1 \"comment | echo 2", 0);

	assert_string_equal("echo 1 \"comment | echo 2", cmds[0]);
	assert_string_equal(NULL, cmds[1]);

	free_string_array(cmds, count_strings(cmds));
}

TEST(no_space_before_first_arg)
{
	char **cmds = break_cmdline(
			"filter!/(важность-(важное|неважное-topics)|срочность-(не)\?срочное)$/",
			0);

	assert_string_equal(
			"filter!/(важность-(важное|неважное-topics)|срочность-(не)\?срочное)$/",
			cmds[0]);
	assert_string_equal(NULL, cmds[1]);

	free_string_array(cmds, count_strings(cmds));
}

TEST(empty_command_at_front)
{
	const char *const COMMANDS = " | if 1 == 1"
	                             " |     let $a = 'a'"
	                             " | endif";

	char **cmds = break_cmdline(COMMANDS, 0);

	assert_string_equal("", cmds[0]);
	assert_string_equal("if 1 == 1 ", cmds[1]);
	assert_string_equal("let $a = 'a' ", cmds[2]);
	assert_string_equal("endif", cmds[3]);
	assert_string_equal(NULL, cmds[4]);

	free_string_array(cmds, count_strings(cmds));
}

TEST(custom_separator_is_parsed_correctly)
{
	char **cmds = break_cmdline("subs!n//|//m!g", 0);

	assert_string_equal("subs!n//|//m!g", cmds[0]);
	assert_string_equal(NULL, cmds[1]);

	free_string_array(cmds, count_strings(cmds));
}

TEST(bar_inside_rarg_is_not_a_separator)
{
	char **cmds = break_cmdline("tr/ ?<>\\\\:*|\"/_", 0);

	assert_string_equal("tr/ ?<>\\\\:*|\"/_", cmds[0]);
	assert_string_equal(NULL, cmds[1]);

	free_string_array(cmds, count_strings(cmds));
}

TEST(bar_after_rarg_is_a_separator)
{
	char **cmds;

	cmds = break_cmdline("select /a|b/ | echo 'hi'", 0);
	assert_string_equal("select /a|b/ ", cmds[0]);
	assert_string_equal("echo 'hi'", cmds[1]);
	assert_string_equal(NULL, cmds[2]);
	free_string_array(cmds, count_strings(cmds));

	cmds = break_cmdline("filter /a|b/ | echo 'hi'", 0);
	assert_string_equal("filter /a|b/ ", cmds[0]);
	assert_string_equal("echo 'hi'", cmds[1]);
	assert_string_equal(NULL, cmds[2]);
	free_string_array(cmds, count_strings(cmds));

	cmds = break_cmdline("s/a/b/g| echo 'hi'", 0);
	assert_string_equal("s/a/b/g", cmds[0]);
	assert_string_equal("echo 'hi'", cmds[1]);
	assert_string_equal(NULL, cmds[2]);
	free_string_array(cmds, count_strings(cmds));

	cmds = break_cmdline("s!a!b!g| echo 'hi'", 0);
	assert_string_equal("s!a!b!g", cmds[0]);
	assert_string_equal("echo 'hi'", cmds[1]);
	assert_string_equal(NULL, cmds[2]);
	free_string_array(cmds, count_strings(cmds));
}

TEST(bar_after_cmd_with_opt_rarg_is_a_separator)
{
	char **cmds;

	cmds = break_cmdline("select *.c | echo 'hi'", 0);
	assert_string_equal("select *.c ", cmds[0]);
	assert_string_equal("echo 'hi'", cmds[1]);
	assert_string_equal(NULL, cmds[2]);
	free_string_array(cmds, count_strings(cmds));

	cmds = break_cmdline("filter *.c | echo 'hi'", 0);
	assert_string_equal("filter *.c ", cmds[0]);
	assert_string_equal("echo 'hi'", cmds[1]);
	assert_string_equal(NULL, cmds[2]);
	free_string_array(cmds, count_strings(cmds));
}

TEST(abbr_consumes_bar)
{
	char **cmds;

	cmds = break_cmdline("cabbrev a before|after", 0);
	assert_string_equal("cabbrev a before|after", cmds[0]);
	assert_string_equal(NULL, cmds[1]);
	free_string_array(cmds, count_strings(cmds));

	cmds = break_cmdline("cnoreabbrev a before|after", 0);
	assert_string_equal("cnoreabbrev a before|after", cmds[0]);
	assert_string_equal(NULL, cmds[1]);
	free_string_array(cmds, count_strings(cmds));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
