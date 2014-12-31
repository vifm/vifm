#include "test.h"

#include "seatest.h"

#include "../../src/utils/macros.h"
#include "../../src/color_manager.h"

void basic_tests(void);

static int colors[128][2];

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

int
main(void)
{
	int result;

	{
		const colmgr_conf_t colmgr_conf = {
			.max_color_pairs = ARRAY_LEN(colors),
			.max_colors = 8,
			.init_pair = &init_pair,
			.pair_content = &pair_content,
		};
		colmgr_init(&colmgr_conf);
	}

	result = run_tests(all_tests);

	return result == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
