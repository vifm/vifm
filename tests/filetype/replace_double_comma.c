#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"

static void
test_no_comma(void)
{
	char buf1[] = "echo something";
	char buf2[] = "echo something";
	replace_double_comma(buf1, 0);
	assert_string_equal("echo something", buf1);
	replace_double_comma(buf2, 1);
	assert_string_equal("echo something", buf2);
}

static void
test_one_command(void)
{
	char buf1[] = "echo tpattern,,with,,comma";
	char buf2[] = "echo tpattern,,with,,comma";
	replace_double_comma(buf1, 0);
	assert_string_equal("echo tpattern,with,comma", buf1);
	replace_double_comma(buf2, 1);
	assert_string_equal("echo tpattern,with,comma", buf2);
}

static void
test_many_commands(void)
{
	char buf1[] = "echo first,,one,echo second,,one";
	char buf2[] = "echo first,,one,echo second,,one";
	replace_double_comma(buf1, 0);
	assert_string_equal("echo first,one,echo second,one", buf1);
	replace_double_comma(buf2, 1);
	assert_string_equal("echo first,one", buf2);
}

void
replace_double_comma_tests(void)
{
	test_fixture_start();

	run_test(test_no_comma);
	run_test(test_one_command);
	run_test(test_many_commands);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
