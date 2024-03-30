#include <stic.h>

#include <curses.h>

#include <limits.h>

#include "../../src/ui/colors.h"
#include "../../src/ui/color_manager.h"
#include "../../src/utils/macros.h"

#include "test.h"

static int count_available_pairs(int seed, int max);

SETUP()
{
	colmgr_reset();
}

TEST(number_of_available_pairs)
{
	assert_int_equal(CUSTOM_COLOR_PAIRS,
			count_available_pairs(INUSE_SEED, INT_MAX));
}

TEST(number_of_available_pairs_after_reset)
{
	(void)count_available_pairs(INUSE_SEED, INT_MAX);
	colmgr_reset();
	assert_int_equal(CUSTOM_COLOR_PAIRS,
			count_available_pairs(INUSE_SEED, INT_MAX));
}

TEST(compression)
{
	assert_int_equal(CUSTOM_COLOR_PAIRS,
			count_available_pairs(UNUSED_SEED, CUSTOM_COLOR_PAIRS));
	assert_int_equal(CUSTOM_COLOR_PAIRS,
			count_available_pairs(INUSE_SEED, INT_MAX));
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

TEST(auto_compression_on_out_of_pairs)
{
	int i;

	/* Mark upper half of custom colors as used. */
	for(i = 0; i < CUSTOM_COLOR_PAIRS/2; ++i)
	{
		assert_int_equal(RESERVED_COLOR_PAIRS + i, colmgr_get_pair(UNUSED_SEED, i));
	}
	for(i = CUSTOM_COLOR_PAIRS/2; i < CUSTOM_COLOR_PAIRS; ++i)
	{
		assert_int_equal(RESERVED_COLOR_PAIRS + i, colmgr_get_pair(INUSE_SEED, i));
	}

	/* Cause compression. */
	assert_int_equal(RESERVED_COLOR_PAIRS + DIV_ROUND_UP(CUSTOM_COLOR_PAIRS, 2),
			colmgr_get_pair(INUSE_SEED, CUSTOM_COLOR_PAIRS));

	/* Verify that all used colors were moved to the bottom half. */
	for(i = RESERVED_COLOR_PAIRS; i < RESERVED_COLOR_PAIRS + CUSTOM_COLOR_PAIRS/2;
			++i)
	{
		assert_int_equal(INUSE_SEED, colors[i][0]);
		assert_int_equal(CUSTOM_COLOR_PAIRS/2 + i, colors[i][1]);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
