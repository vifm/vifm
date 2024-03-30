#include <stic.h>

#include "../../src/ui/color_manager.h"
#include "../../src/utils/macros.h"

#include "test.h"

static int init_pair(int pair, int f, int b);
static int pair_content(int pair, int *f, int *b);
static int pair_in_use(int pair);
static void pair_moved(int from, int to);

int colors[TOTAL_COLOR_PAIRS][3];

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
		.pair_moved = &pair_moved,
	};
	colmgr_init(&colmgr_conf);
}

static int
init_pair(int pair, int f, int b)
{
	colors[pair][0] = f;
	colors[pair][1] = b;
	colors[pair][2] = (f == INUSE_SEED);
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
	return colors[pair][2];
}

static void
pair_moved(int from, int to)
{
	colors[to][2] = 1;
	colors[from][2] = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
