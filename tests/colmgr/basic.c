#include <stic.h>

#include <curses.h>

#include <limits.h>

#include "../../src/ui/colors.h"
#include "../../src/ui/color_manager.h"

#include "test.h"

static int count_available_pairs(int seed, int max);

SETUP()
{
	colmgr_reset();
}

TEST(number_of_available_pairs)
{
	assert_true(count_available_pairs(INUSE_SEED, INT_MAX) == CUSTOM_COLOR_PAIRS);
}

TEST(number_of_available_pairs_after_reset)
{
	(void)count_available_pairs(INUSE_SEED, INT_MAX);
	colmgr_reset();
	assert_true(count_available_pairs(INUSE_SEED, INT_MAX) == CUSTOM_COLOR_PAIRS);
}

TEST(compression)
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

TEST(reuse_of_existing_pair)
{
	assert_int_equal(colmgr_get_pair(1, 1), colmgr_get_pair(1, 1));
}

TEST(negative_fg_and_or_bg)
{
	assert_true(colmgr_get_pair(-1, 0) >= 0);
	assert_true(colmgr_get_pair(0, -1) >= 0);
	assert_true(colmgr_get_pair(-1, -1) >= 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
