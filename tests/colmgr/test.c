#include "test.h"

#include <assert.h>

#include "seatest.h"

#include "../../src/color_manager.h"

void basic_tests(void);

static void
all_tests(void)
{
	basic_tests();
}

int
main(void)
{
	int result;

	initscr();
	assert(has_colors() && "Terminal should support colors.");
	start_color();
	colmgr_init(MAX_COLOR_PAIRS);
	endwin();

	result = run_tests(all_tests);

	return result == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
