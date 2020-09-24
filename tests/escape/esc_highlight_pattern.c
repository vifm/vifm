#include <stic.h>

#include <regex.h>

#include <stdlib.h>

#include "../../src/ui/escape.h"

#define INV_START "\033[7,1m"
#define INV_END "\033[27,22m"

static regex_t re1c;
static regex_t re2c;
static regex_t re3c;
static regex_t re4c;
static regex_t re5c;

SETUP()
{
	assert_success(regcomp(&re1c, "a", 0));
	assert_success(regcomp(&re2c, "ab", 0));
	assert_success(regcomp(&re3c, "$", 0));
	assert_success(regcomp(&re4c, "a?", REG_EXTENDED));
	assert_success(regcomp(&re5c, "^", 0));
}

TEARDOWN()
{
	regfree(&re5c);
	regfree(&re4c);
	regfree(&re3c);
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

TEST(empty_match)
{
	const char *const input = "anything";
	const char *const expected = "anything";
	char *const actual = esc_highlight_pattern(input, &re3c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(match_on_empty_line)
{
	const char *const input = "";
	const char *const expected = "";
	char *const actual = esc_highlight_pattern(input, &re3c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(non_empty_match_after_empty_match)
{
	const char *const input = " a";
	const char *const expected = " " INV_START "a" INV_END;
	char *const actual = esc_highlight_pattern(input, &re4c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(non_empty_match_after_empty_matche_with_esc)
{
	const char *const input = "\033[7,1m" " a";
	const char *const expected = "\033[7,1m" " " INV_START "a" INV_END;
	char *const actual = esc_highlight_pattern(input, &re4c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(empty_match_is_fine)
{
	const char *const input = "foo \033[31m bar vifm";
	const char *const expected = input;
	char *const actual = esc_highlight_pattern(input, &re5c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(trailing_empty_match_is_fine)
{
	const char *const input = "data \033(B\033[m";
	const char *const expected = input;
	char *const actual = esc_highlight_pattern(input, &re3c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(empty_match_with_utf8_character)
{
	const char *const input = "б";
	const char *const expected = input;
	char *const actual = esc_highlight_pattern(input, &re5c);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(inversion_is_recognized)
{
	const char *const input = "a";
	const char *const expected = "\033[7,1m" "a" "\033[27,22m";
	char *const actual = esc_highlight_pattern(input, &re1c);
	assert_string_equal(expected, actual);
	assert_int_equal(6, get_char_width_esc(actual));
	free(actual);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
