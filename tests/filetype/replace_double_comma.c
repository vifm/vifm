#include <stic.h>

#include <stdlib.h>

#include "../../src/filetype.h"

TEST(no_comma)
{
	char buf1[] = "echo something";
	char buf2[] = "echo something";
	replace_double_comma(buf1, 0);
	assert_string_equal("echo something", buf1);
	replace_double_comma(buf2, 1);
	assert_string_equal("echo something", buf2);
}

TEST(one_command)
{
	char buf1[] = "echo tpattern,,with,,comma";
	char buf2[] = "echo tpattern,,with,,comma";
	replace_double_comma(buf1, 0);
	assert_string_equal("echo tpattern,with,comma", buf1);
	replace_double_comma(buf2, 1);
	assert_string_equal("echo tpattern,with,comma", buf2);
}

TEST(many_commands)
{
	char buf1[] = "echo first,,one,echo second,,one";
	char buf2[] = "echo first,,one,echo second,,one";
	replace_double_comma(buf1, 0);
	assert_string_equal("echo first,one,echo second,one", buf1);
	replace_double_comma(buf2, 1);
	assert_string_equal("echo first,one", buf2);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
