#include "seatest.h"

#include "../../src/escape.h"

static void
test_no_esc_zero_overhead(void)
{
	const char *const input = "abcdef";
	assert_int_equal(0, esc_str_overhead(input));
}

static void
test_starts_with_esc_overhead_correct(void)
{
	const char *const input = "\033[39;40m\033[0mdef";
	assert_int_equal(12, esc_str_overhead(input));
}

static void
test_esc_in_the_middle_overhead_correct(void)
{
	const char *const input = "abc\033[39;40m\033[0mdef";
	assert_int_equal(12, esc_str_overhead(input));
}

static void
test_ends_with_esc_overhead_correct(void)
{
	const char *const input = "abc\033[39;40m\033[0m";
	assert_int_equal(12, esc_str_overhead(input));
}

static void
test_many_esc_overhead_correct(void)
{
	const char *const input = "\033[1m\033[32m[\\w]\\$\033[39;40m\033[0m ";
	assert_int_equal(21, esc_str_overhead(input));
}

void
esc_str_overhead_tests(void)
{
	test_fixture_start();

	run_test(test_no_esc_zero_overhead);
	run_test(test_starts_with_esc_overhead_correct);
	run_test(test_esc_in_the_middle_overhead_correct);
	run_test(test_ends_with_esc_overhead_correct);
	run_test(test_many_esc_overhead_correct);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
