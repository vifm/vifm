#include <curses.h>

#include "seatest.h"

#include "../../src/colors.h"
#include "../../src/color_manager.h"

#include "test.h"

static int count_available_pairs(void);

static void
setup(void)
{
	colmgr_reset();
}

static void
test_number_of_available_pairs(void)
{
	assert_true(count_available_pairs() >= MAX_COLOR_PAIRS - FCOLOR_BASE);
}

static void
test_number_of_available_pairs_after_reset(void)
{
	(void)count_available_pairs();
	colmgr_reset();
	assert_true(count_available_pairs() >= MAX_COLOR_PAIRS - FCOLOR_BASE);
}

static int
count_available_pairs(void)
{
	int i = 0;
	while(colmgr_alloc_pair(i, i) != -1)
	{
		i++;
	}
	return i;
}

static void
test_reuse_of_existing_pair(void)
{
	assert_int_equal(colmgr_alloc_pair(1, 1), colmgr_alloc_pair(1, 1));
}

void
basic_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_number_of_available_pairs);
	run_test(test_number_of_available_pairs_after_reset);
	run_test(test_reuse_of_existing_pair);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
