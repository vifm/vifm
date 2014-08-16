#include "seatest.h"

#include <ctype.h> /* isdigit() */

#include "../../src/utils/macros.h"

static int
less_than_5(int x)
{
    return x < 5;
}

void
test_all_all_are_true(void)
{
	assert_int_equal(1, ALL(less_than_5, 1, -2, 3));
	assert_int_equal(1, ALL(isdigit, '1', '9'));
}

void
test_all_all_are_false(void)
{
	assert_int_equal(0, ALL(less_than_5, 10, 20, 30));
}

void
test_all_one_is_true(void)
{
	assert_int_equal(0, ALL(less_than_5, 10, 0, 30));
	assert_int_equal(0, ALL(isdigit, 'z', '9'));
}

void
test_none_all_are_true(void)
{
	assert_int_equal(0, NONE(less_than_5, 1, -2, 3));
	assert_int_equal(0, NONE(isdigit, '1', '9'));
}

void
test_none_all_are_false(void)
{
	assert_int_equal(1, NONE(less_than_5, 10, 20, 30));
	assert_int_equal(1, NONE(isdigit, 'z', 'a'));
}

void
test_none_one_is_true(void)
{
	assert_int_equal(0, NONE(less_than_5, 10, 0, 30));
}

void
test_any_all_are_true(void)
{
	assert_int_equal(1, ANY(less_than_5, 1, -2, 3));
}

void
test_any_all_are_false(void)
{
	assert_int_equal(0, ANY(less_than_5, 10, 20, 30));
	assert_int_equal(0, ANY(isdigit, 'z', 'a'));
}

void
test_any_one_is_true(void)
{
	assert_int_equal(1, ANY(less_than_5, 10, 0, 30));
	assert_int_equal(1, ANY(isdigit, 'z', '9'));
}

void
functional_tests(void)
{
	test_fixture_start();

	run_test(test_all_all_are_true);
	run_test(test_all_all_are_false);
	run_test(test_all_one_is_true);

	run_test(test_none_all_are_true);
	run_test(test_none_all_are_false);
	run_test(test_none_one_is_true);

	run_test(test_any_all_are_true);
	run_test(test_any_all_are_false);
	run_test(test_any_one_is_true);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
