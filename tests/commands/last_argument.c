#include <stic.h>

#include <string.h>

#include "../../src/engine/cmds.h"

extern cmds_conf_t cmds_conf;

cmd_info_t user_cmd_info;

TEST(empty_ok)
{
	const char cmd[] = "";
	size_t len;
	const char *last;
	last = vle_cmds_last_arg(cmd, 0, &len);

	assert_true(last == cmd);
	assert_int_equal(0, len);
}

TEST(one_word_ok)
{
	const char cmd[] = "a";
	size_t len;
	const char *last;
	last = vle_cmds_last_arg(cmd, 0, &len);

	assert_true(last == cmd);
	assert_int_equal(1, len);
}

TEST(two_words_ok)
{
	const char cmd[] = "b a";
	size_t len;
	const char *last;
	last = vle_cmds_last_arg(cmd, 0, &len);

	assert_true(last == cmd + 2);
	assert_int_equal(1, len);
}

TEST(trailing_spaces_ok)
{
	const char cmd[] = "a    ";
	size_t len;
	const char *last;
	last = vle_cmds_last_arg(cmd, 0, &len);

	assert_true(last == cmd);
	assert_int_equal(1, len);
}

TEST(single_quotes_ok)
{
	const char cmd[] = "b  'hello'";
	size_t len;
	const char *last;
	last = vle_cmds_last_arg(cmd, 0, &len);

	assert_true(last == cmd + 3);
	assert_int_equal(7, len);
}

TEST(double_quotes_ok)
{
	const char cmd[] = "b \"hi\"";
	size_t len;
	const char *last;
	last = vle_cmds_last_arg(cmd, 0, &len);

	assert_true(last == cmd + 2);
	assert_int_equal(4, len);
}

TEST(ending_space_ok)
{
	const char cmd[] = "b a\\ ";
	size_t len;
	const char *last;
	last = vle_cmds_last_arg(cmd, 0, &len);

	assert_true(last == cmd + 2);
	assert_int_equal(3, len);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
