#include <stic.h>

#include "../../src/utils/str.h"

TEST(empty_strings)
{
	assert_false(is_in_str_list("", '\0', ""));
	assert_false(is_in_str_list("", ',', ""));
}

TEST(empty_element)
{
	assert_false(is_in_str_list("a,b", ',', ""));

	assert_false(is_in_str_list("a,,b", ',', ""));
	assert_false(is_in_str_list(",a,b", ',', ""));
	assert_false(is_in_str_list("a,b,", ',', ""));
}

TEST(singe_character_string)
{
	assert_false(is_in_str_list("a,b,c", ',', "x"));

	assert_true(is_in_str_list("a,b,c", ',', "a"));
	assert_true(is_in_str_list("a,b,c", ',', "b"));
	assert_true(is_in_str_list("a,b,c", ',', "c"));
}

TEST(multi_character_string)
{
	assert_false(is_in_str_list("aa,bb,cc", ',', "xx"));

	assert_false(is_in_str_list("aa,bb,cc", ',', "a"));
	assert_false(is_in_str_list("aa,bb,cc", ',', "b"));
	assert_false(is_in_str_list("aa,bb,cc", ',', "c"));

	assert_true(is_in_str_list("aa,bb,cc", ',', "aa"));
	assert_true(is_in_str_list("aa,bb,cc", ',', "bb"));
	assert_true(is_in_str_list("aa,bb,cc", ',', "cc"));
}

TEST(non_comma_separator)
{
	assert_false(is_in_str_list("aa|bb|cc", '|', "xx"));
	assert_true(is_in_str_list("aa|bb|cc", '|', "aa"));

	assert_false(is_in_str_list("aa=bb=cc", '=', "xx"));
	assert_true(is_in_str_list("aa=bb=cc", '=', "aa"));

	assert_false(is_in_str_list("aa*bb=cc", '*', "xx"));
	assert_true(is_in_str_list("aa*bb=cc", '*', "aa"));
}

TEST(case_insensitive)
{
	assert_true(is_in_str_list("aA,BB,Cc", ',', "aa"));
	assert_true(is_in_str_list("aA,BB,Cc", ',', "bb"));
	assert_true(is_in_str_list("aA,BB,Cc", ',', "cc"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
