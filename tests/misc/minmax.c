#include <limits.h> /* INT_MIN INT_MAX */

#include "seatest.h"

#include "../../src/utils/macros.h"

void
test_min_with_negative_numbers(void)
{
	assert_int_equal(-99, MIN(-50, -99));
}

void
test_min_with_positive_numbers(void)
{
	assert_int_equal(56, MIN(56, 82));
}

void
test_min_with_posneg_numbers(void)
{
	assert_int_equal(-6111, MIN(7222, -6111));
}

void
test_min_with_negpos_numbers(void)
{
	assert_int_equal(-900, MIN(-900, 622));
}

void
test_min_with_extreme_numbers(void)
{
	assert_int_equal(INT_MIN, MIN(INT_MIN, INT_MAX));
}

void
test_max_with_negative_numbers(void)
{
	assert_int_equal(-1211, MAX(-23423, -1211));
}

void
test_max_with_positive_numbers(void)
{
	assert_int_equal(193775235, MAX(146719, 193775235));
}

void
test_max_with_posneg_numbers(void)
{
	assert_int_equal(146719, MAX(146719, -1951));
}

void
test_max_with_negpos_numbers(void)
{
	assert_int_equal(12384, MAX(-18239547, 12384));
}

void
test_max_with_extreme_numbers(void)
{
	assert_int_equal(INT_MAX, MAX(INT_MIN, INT_MAX));
}

void
minmax_tests(void)
{
	test_fixture_start();

	run_test(test_min_with_negative_numbers);
	run_test(test_min_with_positive_numbers);
	run_test(test_min_with_posneg_numbers);
	run_test(test_min_with_negpos_numbers);
	run_test(test_min_with_extreme_numbers);

	run_test(test_max_with_negative_numbers);
	run_test(test_max_with_positive_numbers);
	run_test(test_max_with_posneg_numbers);
	run_test(test_max_with_negpos_numbers);
	run_test(test_max_with_extreme_numbers);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
