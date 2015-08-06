#include <stic.h>

#include <limits.h> /* INT_MIN INT_MAX */

#include "../../src/utils/macros.h"

TEST(min_with_negative_numbers)
{
	assert_int_equal(-99, MIN(-50, -99));
}

TEST(min_with_positive_numbers)
{
	assert_int_equal(56, MIN(56, 82));
}

TEST(min_with_posneg_numbers)
{
	assert_int_equal(-6111, MIN(7222, -6111));
}

TEST(min_with_negpos_numbers)
{
	assert_int_equal(-900, MIN(-900, 622));
}

TEST(min_with_extreme_numbers)
{
	assert_int_equal(INT_MIN, MIN(INT_MIN, INT_MAX));
}

TEST(max_with_negative_numbers)
{
	assert_int_equal(-1211, MAX(-23423, -1211));
}

TEST(max_with_positive_numbers)
{
	assert_int_equal(193775235, MAX(146719, 193775235));
}

TEST(max_with_posneg_numbers)
{
	assert_int_equal(146719, MAX(146719, -1951));
}

TEST(max_with_negpos_numbers)
{
	assert_int_equal(12384, MAX(-18239547, 12384));
}

TEST(max_with_extreme_numbers)
{
	assert_int_equal(INT_MAX, MAX(INT_MIN, INT_MAX));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
