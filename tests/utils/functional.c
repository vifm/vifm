#include <stic.h>

#include <ctype.h> /* isdigit() */

#include "../../src/utils/macros.h"

static int
less_than_5(int x)
{
    return x < 5;
}

TEST(all_all_are_true)
{
	assert_int_equal(1, ALL(less_than_5, 1, -2, 3));
	assert_int_equal(1, ALL(isdigit, '1', '9'));
}

TEST(all_all_are_false)
{
	assert_int_equal(0, ALL(less_than_5, 10, 20, 30));
}

TEST(all_one_is_true)
{
	assert_int_equal(0, ALL(less_than_5, 10, 0, 30));
	assert_int_equal(0, ALL(isdigit, 'z', '9'));
}

TEST(none_all_are_true)
{
	assert_int_equal(0, NONE(less_than_5, 1, -2, 3));
	assert_int_equal(0, NONE(isdigit, '1', '9'));
}

TEST(none_all_are_false)
{
	assert_int_equal(1, NONE(less_than_5, 10, 20, 30));
	assert_int_equal(1, NONE(isdigit, 'z', 'a'));
}

TEST(none_one_is_true)
{
	assert_int_equal(0, NONE(less_than_5, 10, 0, 30));
}

TEST(any_all_are_true)
{
	assert_int_equal(1, ANY(less_than_5, 1, -2, 3));
}

TEST(any_all_are_false)
{
	assert_int_equal(0, ANY(less_than_5, 10, 20, 30));
	assert_int_equal(0, ANY(isdigit, 'z', 'a'));
}

TEST(any_one_is_true)
{
	assert_int_equal(1, ANY(less_than_5, 10, 0, 30));
	assert_int_equal(1, ANY(isdigit, 'z', '9'));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
