#include <stdlib.h>

#include "seatest.h"

#include "../../src/escape.h"

static void
test_no_esc_string_untouched(void)
{
	const char *const input = "abcdef";
	const char *const expected = "abcdef";
	char *const actual = esc_remove(input);
	assert_string_equal(expected, actual);
	free(actual);
}

static void
test_starts_with_esc_removed_ok(void)
{
	const char *const input = "\033[39;40m\033[0mdef";
	const char *const expected = "def";
	char *const actual = esc_remove(input);
	assert_string_equal(expected, actual);
	free(actual);
}

static void
test_esc_in_the_middle_removed_ok(void)
{
	const char *const input = "abc\033[39;40m\033[0mdef";
	const char *const expected = "abcdef";
	char *const actual = esc_remove(input);
	assert_string_equal(expected, actual);
	free(actual);
}

static void
test_ends_with_esc_removed_ok(void)
{
	const char *const input = "abc\033[39;40m\033[0m";
	const char *const expected = "abc";
	char *const actual = esc_remove(input);
	assert_string_equal(expected, actual);
	free(actual);
}

static void
test_many_esc_removed_ok(void)
{
	const char *const input = "\033[1m\033[32m[\\w]\\$\033[39;40m\033[0m ";
	const char *const expected = "[\\w]\\$ ";
	char *const actual = esc_remove(input);
	assert_string_equal(expected, actual);
	free(actual);
}

void
esc_remove_tests(void)
{
	test_fixture_start();

	run_test(test_no_esc_string_untouched);
	run_test(test_starts_with_esc_removed_ok);
	run_test(test_esc_in_the_middle_removed_ok);
	run_test(test_ends_with_esc_removed_ok);
	run_test(test_many_esc_removed_ok);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
