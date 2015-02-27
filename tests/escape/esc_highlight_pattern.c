#include <stic.h>

#include <regex.h>

#include <stdlib.h>

#include "../../src/escape.h"

static regex_t re1c;
static regex_t re2c;

SETUP()
{
	int err;

	err = regcomp(&re1c, "a", 0);
	assert_int_equal(0, err);
	err = regcomp(&re2c, "ab", 0);
	assert_int_equal(0, err);
}

TEARDOWN()
{
	regfree(&re2c);
	regfree(&re1c);
}

TEST(single_char_regex_ok)
{
	const char *const input = "a";
	const char *const expected = "\033[7,1m" "a" "\033[27,22m";
	char *const actual = esc_highlight_pattern(input, &re1c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(double_char_regex_ok)
{
	const char *const input = "ab";
	const char *const expected = "\033[7,1m" "a" "\033[27,22m" "\033[7,1m" "b" "\033[27,22m";
	char *const actual = esc_highlight_pattern(input, &re2c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(match_at_the_beginning_ok)
{
	const char *const input = "abBAR";
	const char *const expected = "\033[7,1m" "a" "\033[27,22m" "\033[7,1m" "b" "\033[27,22mBAR";
	char *const actual = esc_highlight_pattern(input, &re2c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(match_at_the_end_ok)
{
	const char *const input = "FOOab";
	const char *const expected = "FOO" "\033[7,1m" "a" "\033[27,22m" "\033[7,1m" "b" "\033[27,22m";
	char *const actual = esc_highlight_pattern(input, &re2c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(match_in_the_middle_ok)
{
	const char *const input = "FOOabBAR";
	const char *const expected = "FOO" "\033[7,1m" "a" "\033[27,22m" "\033[7,1m" "b" "\033[27,22m" "BAR";
	char *const actual = esc_highlight_pattern(input, &re2c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(multiple_matches_ok)
{
	const char *const input = "FOOabBARab";
	const char *const expected = "FOO" "\033[7,1m" "a" "\033[27,22m" "\033[7,1m" "b" "\033[27,22m"
	                             "BAR" "\033[7,1m" "a" "\033[27,22m" "\033[7,1m" "b" "\033[27,22m";
	char *const actual = esc_highlight_pattern(input, &re2c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(esc_in_input_are_ignored)
{
	const char *const input = "FOO" "\033[7,1m" "a" "\033[27,22m" "BAR";
	const char *const expected = "FOO" "\033[7,1m" "\033[7,1m" "a" "\033[27,22m" "\033[27,22m" "BAR";
	char *const actual = esc_highlight_pattern(input, &re1c);
	assert_string_equal(expected, actual);
	free(actual);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
