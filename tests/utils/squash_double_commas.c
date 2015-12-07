#include <stic.h>

#include "../../src/utils/str.h"

TEST(no_comma)
{
	char buf[] = "echo something";
	squash_double_commas(buf);
	assert_string_equal("echo something", buf);
}

TEST(one_command)
{
	char buf[] = "echo tpattern,,with,,comma";
	squash_double_commas(buf);
	assert_string_equal("echo tpattern,with,comma", buf);
}

TEST(many_commands)
{
	char buf[] = "echo first,,one,echo second,,one";
	squash_double_commas(buf);
	assert_string_equal("echo first,one,echo second,one", buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
