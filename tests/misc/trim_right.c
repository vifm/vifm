#include "seatest.h"

#include "../../src/utils/str.h"

static void
test_empty_string(void)
{
	char buf[] = "";
	trim_right(buf);
	assert_string_equal("", buf);
}

static void
test_spaces_only(void)
{
	char buf[] = "   \t\t\t   ";
	trim_right(buf);
	assert_string_equal("", buf);
}

static void
test_one_space_char(void)
{
	char buf[] = "aab ";
	trim_right(buf);
	assert_string_equal("aab", buf);
}

static void
test_many_space_chars(void)
{
	char buf[] = "ab  \t\t";
	trim_right(buf);
	assert_string_equal("ab", buf);
}

void
trim_right_tests(void)
{
	test_fixture_start();

	run_test(test_empty_string);
	run_test(test_spaces_only);
	run_test(test_one_space_char);
	run_test(test_many_space_chars);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
