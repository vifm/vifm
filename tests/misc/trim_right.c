#include <stic.h>

#include "../../src/int/vim.h"

TEST(empty_string)
{
	char buf[] = "";
	trim_right(buf);
	assert_string_equal("", buf);
}

TEST(spaces_only)
{
	char buf[] = "   \t\t\t   ";
	trim_right(buf);
	assert_string_equal("", buf);
}

TEST(one_space_char)
{
	char buf[] = "aab ";
	trim_right(buf);
	assert_string_equal("aab", buf);
}

TEST(many_space_chars)
{
	char buf[] = "ab  \t\t";
	trim_right(buf);
	assert_string_equal("ab", buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
