#include <stic.h>

#include "../../src/ui/color_manager.h"
#include "../../src/utils/macros.h"

#include "test.h"

static int init_pair(int pair, int f, int b);
static int pair_content(int pair, int *f, int *b);
static int pair_in_use(int pair);
static void move_pair(int from, int to);

static int colors[TOTAL_COLOR_PAIRS][2];

DEFINE_SUITE();

SETUP()
{
	colmgr_reset();
}

SETUP_ONCE()
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

static int
init_pair(int pair, int f, int b)
{
	colors[pair][0] = f;
	colors[pair][1] = b;
	return 0;
}

static int
pair_content(int pair, int *f, int *b)
{
	*f = colors[pair][0];
	*b = colors[pair][1];
	return 0;
}

static int
pair_in_use(int pair)
{
	return colors[pair][0] == INUSE_SEED;
}

static void
move_pair(int from, int to)
{
	colors[to][0] = colors[from][0];
	colors[to][1] = colors[from][1];

	colors[from][0] = UNUSED_SEED;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
