#include <regex.h>

#include <stdlib.h>

#include "seatest.h"

#include "../../src/escape.h"

static regex_t re1c;
static regex_t re2c;

static void
setup(void)
{
	int err;
	err = regcomp(&re1c, "a", 0);
	assert_int_equal(0, err);
	err = regcomp(&re2c, "ab", 0);
	assert_int_equal(0, err);
}

static void
teardown(void)
{
	regfree(&re2c);
	regfree(&re1c);
}

static void
test_single_char_regex_ok(void)
{
	const char *const input = "a";
	const char *const expected = "\033[7,1m" "a" "\033[27,22m";
	char *const actual = esc_highlight_pattern(input, &re1c);
	assert_string_equal(expected, actual);
	free(actual);
}

static void
test_double_char_regex_ok(void)
{
	const char *const input = "ab";
	const char *const expected = "\033[7,1m" "a" "\033[27,22m" "\033[7,1m" "b" "\033[27,22m";
	char *const actual = esc_highlight_pattern(input, &re2c);
	assert_string_equal(expected, actual);
	free(actual);
}

static void
test_match_at_the_beginning_ok(void)
{
	const char *const input = "abBAR";
	const char *const expected = "\033[7,1m" "a" "\033[27,22m" "\033[7,1m" "b" "\033[27,22mBAR";
	char *const actual = esc_highlight_pattern(input, &re2c);
	assert_string_equal(expected, actual);
	free(actual);
}

static void
test_match_at_the_end_ok(void)
{
	const char *const input = "FOOab";
	const char *const expected = "FOO" "\033[7,1m" "a" "\033[27,22m" "\033[7,1m" "b" "\033[27,22m";
	char *const actual = esc_highlight_pattern(input, &re2c);
	assert_string_equal(expected, actual);
	free(actual);
}

static void
test_match_in_the_middle_ok(void)
{
	const char *const input = "FOOabBAR";
	const char *const expected = "FOO" "\033[7,1m" "a" "\033[27,22m" "\033[7,1m" "b" "\033[27,22m" "BAR";
	char *const actual = esc_highlight_pattern(input, &re2c);
	assert_string_equal(expected, actual);
	free(actual);
}

static void
test_multiple_matches_ok(void)
{
	const char *const input = "FOOabBARab";
	const char *const expected = "FOO" "\033[7,1m" "a" "\033[27,22m" "\033[7,1m" "b" "\033[27,22m"
	                             "BAR" "\033[7,1m" "a" "\033[27,22m" "\033[7,1m" "b" "\033[27,22m";
	char *const actual = esc_highlight_pattern(input, &re2c);
	assert_string_equal(expected, actual);
	free(actual);
}

static void
test_esc_in_input_are_ignored(void)
{
	const char *const input = "FOO" "\033[7,1m" "a" "\033[27,22m" "BAR";
	const char *const expected = "FOO" "\033[7,1m" "\033[7,1m" "a" "\033[27,22m" "\033[27,22m" "BAR";
	char *const actual = esc_highlight_pattern(input, &re1c);
	assert_string_equal(expected, actual);
	free(actual);
}

void
esc_highlight_pattern_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_single_char_regex_ok);
	run_test(test_double_char_regex_ok);
	run_test(test_match_at_the_beginning_ok);
	run_test(test_match_at_the_end_ok);
	run_test(test_match_in_the_middle_ok);
	run_test(test_multiple_matches_ok);
	run_test(test_esc_in_input_are_ignored);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
