#include <stic.h>

#include <stdlib.h>

#include "../../src/ui/escape.h"

TEST(no_esc_string_untouched)
{
	const char *const input = "abcdef";
	const char *const expected = "abcdef";
	char *const actual = esc_remove(input);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(starts_with_esc_removed_ok)
{
	const char *const input = "\033[39;40m\033[0mdef";
	const char *const expected = "def";
	char *const actual = esc_remove(input);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(esc_in_the_middle_removed_ok)
{
	const char *const input = "abc\033[39;40m\033[0mdef";
	const char *const expected = "abcdef";
	char *const actual = esc_remove(input);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(ends_with_esc_removed_ok)
{
	const char *const input = "abc\033[39;40m\033[0m";
	const char *const expected = "abc";
	char *const actual = esc_remove(input);
	assert_string_equal(expected, actual);
	free(actual);
}

TEST(many_esc_removed_ok)
{
	const char *const input = "\033[1m\033[32m[\\w]\\$\033[39;40m\033[0m ";
	const char *const expected = "[\\w]\\$ ";
	char *const actual = esc_remove(input);
	assert_string_equal(expected, actual);
	free(actual);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
