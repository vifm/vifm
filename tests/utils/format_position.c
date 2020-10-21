#include <stic.h>

#include "../../src/utils/utils.h"

TEST(all)
{
	char buf[32];

	format_position(buf, sizeof(buf), 0, 1, 10);
	assert_string_equal("All", buf);

	format_position(buf, sizeof(buf), 0, 9, 10);
	assert_string_equal("All", buf);

	format_position(buf, sizeof(buf), 0, 10, 10);
	assert_string_equal("All", buf);
}

TEST(top)
{
	char buf[32];

	format_position(buf, sizeof(buf), 0, 11, 10);
	assert_string_equal("Top", buf);

	format_position(buf, sizeof(buf), 0, 100, 10);
	assert_string_equal("Top", buf);
}

TEST(bottom)
{
	char buf[32];

	format_position(buf, sizeof(buf), 1, 11, 10);
	assert_string_equal("Bot", buf);

	format_position(buf, sizeof(buf), 1, 101, 100);
	assert_string_equal("Bot", buf);
}

TEST(percent)
{
	char buf[32];

	format_position(buf, sizeof(buf), 1, 12, 10);
	assert_string_equal("50%", buf);

	format_position(buf, sizeof(buf), 1, 13, 10);
	assert_string_equal("33%", buf);

	format_position(buf, sizeof(buf), 2, 13, 10);
	assert_string_equal("66%", buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
