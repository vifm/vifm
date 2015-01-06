#include "seatest.h"

#include "../../src/utils/str.h"

static void
test_empty_strings(void)
{
	assert_false(is_in_str_list("", '\0', ""));
	assert_false(is_in_str_list("", ',', ""));
}

static void
test_empty_element(void)
{
	assert_false(is_in_str_list("a,b", ',', ""));

	assert_false(is_in_str_list("a,,b", ',', ""));
	assert_false(is_in_str_list(",a,b", ',', ""));
	assert_false(is_in_str_list("a,b,", ',', ""));
}

static void
test_singe_character_string(void)
{
	assert_false(is_in_str_list("a,b,c", ',', "x"));

	assert_true(is_in_str_list("a,b,c", ',', "a"));
	assert_true(is_in_str_list("a,b,c", ',', "b"));
	assert_true(is_in_str_list("a,b,c", ',', "c"));
}

static void
test_multi_character_string(void)
{
	assert_false(is_in_str_list("aa,bb,cc", ',', "xx"));

	assert_false(is_in_str_list("aa,bb,cc", ',', "a"));
	assert_false(is_in_str_list("aa,bb,cc", ',', "b"));
	assert_false(is_in_str_list("aa,bb,cc", ',', "c"));

	assert_true(is_in_str_list("aa,bb,cc", ',', "aa"));
	assert_true(is_in_str_list("aa,bb,cc", ',', "bb"));
	assert_true(is_in_str_list("aa,bb,cc", ',', "cc"));
}

static void
test_non_comma_separator(void)
{
	assert_false(is_in_str_list("aa|bb|cc", '|', "xx"));
	assert_true(is_in_str_list("aa|bb|cc", '|', "aa"));

	assert_false(is_in_str_list("aa=bb=cc", '=', "xx"));
	assert_true(is_in_str_list("aa=bb=cc", '=', "aa"));

	assert_false(is_in_str_list("aa*bb=cc", '*', "xx"));
	assert_true(is_in_str_list("aa*bb=cc", '*', "aa"));
}

static void
test_case_insensitive(void)
{
	assert_true(is_in_str_list("aA,BB,Cc", ',', "aa"));
	assert_true(is_in_str_list("aA,BB,Cc", ',', "bb"));
	assert_true(is_in_str_list("aA,BB,Cc", ',', "cc"));
}

void
is_in_str_list_tests(void)
{
	test_fixture_start();

	run_test(test_empty_strings);
	run_test(test_empty_element);
	run_test(test_singe_character_string);
	run_test(test_multi_character_string);
	run_test(test_non_comma_separator);
	run_test(test_case_insensitive);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
