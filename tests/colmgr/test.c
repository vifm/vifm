#include "test.h"

#include "seatest.h"

#include "../../src/utils/macros.h"
#include "../../src/color_manager.h"

void basic_tests(void);

static int colors[TOTAL_COLOR_PAIRS][2];

static void
setup(void)
{
	colmgr_reset();
}

static void
all_tests(void)
{
	basic_tests();
}

static int
init_pair(short pair, short f, short b)
{
	colors[pair][0] = f;
	colors[pair][1] = b;
	return 0;
}

static int
pair_content(short pair, short *f, short *b)
{
	*f = colors[pair][0];
	*b = colors[pair][1];
	return 0;
}

static int
pair_in_use(short int pair)
{
	return colors[pair][0] == INUSE_SEED;
}

static void
move_pair(short int from, short int to)
{
	colors[to][0] = colors[from][0];
	colors[to][1] = colors[from][1];

	colors[from][0] = UNUSED_SEED;
}

int
main(void)
{
	int result;

	{
		const colmgr_conf_t colmgr_conf =
		{
			.max_color_pairs = ARRAY_LEN(colors),
			.max_colors = 8,
			.init_pair = &init_pair,
			.pair_content = &pair_content,
			.pair_in_use = &pair_in_use,
			.move_pair = &move_pair,
		};
		colmgr_init(&colmgr_conf);
	}

	suite_setup(setup);
	result = run_tests(all_tests);

	return result == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
