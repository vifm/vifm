#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/engine/cmds.h"
#include "../../src/cmd_core.h"

SETUP()
{
	init_commands();
}

TEARDOWN()
{
	reset_cmds();
}

TEST(pipe)
{
	const char *buf;

	buf = "filter /a|b/";
	assert_int_equal(0, line_pos(buf, buf, ' ', 1));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 1));
	assert_int_equal(5, line_pos(buf, buf + 9, ' ', 1));

	buf = "filter 'a|b'";
	assert_int_equal(0, line_pos(buf, buf, ' ', 1));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 1));
	assert_int_equal(3, line_pos(buf, buf + 9, ' ', 1));

	buf = "filter \"a|b\"";
	assert_int_equal(0, line_pos(buf, buf, ' ', 1));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 1));
	assert_int_equal(4, line_pos(buf, buf + 9, ' ', 1));
}

TEST(two_commands)
{
	const char buf[] = "apropos|locate";

	assert_int_equal(0, line_pos(buf, buf, ' ', 0));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 0));
	assert_int_equal(0, line_pos(buf, buf + 7, ' ', 0));
}

TEST(set_command)
{
	const char *buf;

	buf = "set fusehome=\"a|b\"";
	assert_int_equal(0, line_pos(buf, buf, ' ', 0));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 0));
	assert_int_equal(4, line_pos(buf, buf + 16, ' ', 0));

	buf = "set fusehome='a|b'";
	assert_int_equal(0, line_pos(buf, buf, ' ', 0));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 0));
	assert_int_equal(3, line_pos(buf, buf + 16, ' ', 0));
}

TEST(skip)
{
	const char *buf;

	buf = "set fusehome=a\\|b";
	assert_int_equal(0, line_pos(buf, buf, ' ', 0));
	assert_int_equal(0, line_pos(buf, buf + 1, ' ', 0));
	assert_int_equal(1, line_pos(buf, buf + 15, ' ', 0));
}

TEST(custom_separator)
{
	const char *buf;

	buf = "s/a|b\\/c/d|e/g|";
	assert_int_equal(0, line_pos(buf, buf, '/', 1));
	assert_int_equal(0, line_pos(buf, buf + 1, '/', 1));
	assert_int_equal(2, line_pos(buf, buf + 2, '/', 1));
	assert_int_equal(2, line_pos(buf, buf + 3, '/', 1));
	assert_int_equal(2, line_pos(buf, buf + 4, '/', 1));
	assert_int_equal(1, line_pos(buf, buf + 6, '/', 1));
	assert_int_equal(2, line_pos(buf, buf + 10, '/', 1));
	assert_int_equal(0, line_pos(buf, buf + 14, '/', 1));
}

TEST(space_amp_before_bar)
{
	const char buf[] = "apropos &|locate";

	assert_int_equal(0, line_pos(buf, buf, ' ', 0));
	assert_int_equal(0, line_pos(buf, buf + 7, ' ', 0));
	assert_int_equal(0, line_pos(buf, buf + 8, ' ', 0));
	assert_int_equal(0, line_pos(buf, buf + 9, ' ', 0));
}

TEST(whole_line_command_cmdline_is_not_broken)
{
	char **cmds = break_cmdline("!echo hi|less", 0);

	assert_string_equal("!echo hi|less", cmds[0]);
	assert_string_equal(NULL, cmds[1]);

	while(*cmds != NULL)
	{
		free(*cmds++);
	}
}

TEST(bar_escaping_is_preserved_for_whole_line_commands)
{
	char **cmds = break_cmdline("!\\|\\||\\|\\|", 0);
	void *free_this = cmds;

	assert_string_equal("!\\|\\||\\|\\|", cmds[0]);
	assert_string_equal(NULL, cmds[1]);

	while(*cmds != NULL)
	{
		free(*cmds++);
	}
	free(free_this);
}

TEST(asdf)
{
	char **cmds = break_cmdline("echo 1 \\|| 2", 0);
	void *free_this = cmds;

	assert_string_equal("echo 1 \\|| 2", cmds[0]);
	assert_string_equal(NULL, cmds[1]);

	while(*cmds != NULL)
	{
		free(*cmds++);
	}
	free(free_this);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
