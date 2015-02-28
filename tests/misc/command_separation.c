#include <stic.h>

#include "../../src/commands.h"

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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
