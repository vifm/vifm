#include <stic.h>

#include "../../src/ui/escape.h"

TEST(no_esc_zero_overhead)
{
	const char *const input = "abcdef";
	assert_int_equal(0, esc_str_overhead(input));
}

TEST(starts_with_esc_overhead_correct)
{
	const char *const input = "\033[39;40m\033[0mdef";
	assert_int_equal(12, esc_str_overhead(input));
}

TEST(esc_in_the_middle_overhead_correct)
{
	const char *const input = "abc\033[39;40m\033[0mdef";
	assert_int_equal(12, esc_str_overhead(input));
}

TEST(ends_with_esc_overhead_correct)
{
	const char *const input = "abc\033[39;40m\033[0m";
	assert_int_equal(12, esc_str_overhead(input));
}

TEST(many_esc_overhead_correct)
{
	const char *const input = "\033[1m\033[32m[\\w]\\$\033[39;40m\033[0m ";
	assert_int_equal(21, esc_str_overhead(input));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
