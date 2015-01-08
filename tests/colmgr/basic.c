#include <curses.h>

#include <limits.h>

#include "seatest.h"

#include "../../src/colors.h"
#include "../../src/color_manager.h"

#include "test.h"

static int count_available_pairs(int seed, int max);

static void
setup(void)
{
	colmgr_reset();
}

static void
test_number_of_available_pairs(void)
{
	assert_true(count_available_pairs(INUSE_SEED, INT_MAX) == CUSTOM_COLOR_PAIRS);
}

static void
test_number_of_available_pairs_after_reset(void)
{
	(void)count_available_pairs(INUSE_SEED, INT_MAX);
	colmgr_reset();
	assert_true(count_available_pairs(INUSE_SEED, INT_MAX) == CUSTOM_COLOR_PAIRS);
}

static void
test_compression(void)
{
	assert_true(
			count_available_pairs(UNUSED_SEED, CUSTOM_COLOR_PAIRS)
			==
			CUSTOM_COLOR_PAIRS
	);
	assert_true(count_available_pairs(INUSE_SEED, INT_MAX) == CUSTOM_COLOR_PAIRS);
}

static int
count_available_pairs(int seed, int max)
{
	int i = 0;
	while(i < max && colmgr_get_pair(seed, i) != 0)
	{
		++i;
	}
	return i;
}

static void
test_reuse_of_existing_pair(void)
{
	assert_int_equal(colmgr_get_pair(1, 1), colmgr_get_pair(1, 1));
}

static void
test_negative_fg_and_or_bg(void)
{
	assert_true(colmgr_get_pair(-1, 0) >= 0);
	assert_true(colmgr_get_pair(0, -1) >= 0);
	assert_true(colmgr_get_pair(-1, -1) >= 0);
}

void
basic_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_number_of_available_pairs);
	run_test(test_number_of_available_pairs_after_reset);
	run_test(test_compression);
	run_test(test_reuse_of_existing_pair);
	run_test(test_negative_fg_and_or_bg);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
